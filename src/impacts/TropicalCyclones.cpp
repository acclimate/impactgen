/*
  Copyright (C) 2019 Sven Willner <sven.willner@pik-potsdam.de>

  This file is part of the Acclimate ImpactGen.

  Acclimate ImpactGen is free software: you can redistribute it and/or
  modify it under the terms of the GNU Affero General Public License
  as published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Acclimate ImpactGen is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public
  License along with Acclimate ImpactGen.  If not, see
  <http://www.gnu.org/licenses/>.
*/

#include "impacts/TropicalCyclones.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include "Output.h"
#include "TimeVariable.h"
#include "helpers.h"
#include "netcdftools.h"
#include "nvector.h"
#include "progressbar.h"

namespace impactgen {

static const std::array<int, 13> cumulative_days_per_month = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

template<typename T>
inline T distance(T lon1, T lat1, T lon2, T lat2) {
    static constexpr T r = 6371;
    static constexpr T pi = std::acos(-1);
    const T sqrt_hav_lat = std::sin((lat1 - lat2) / 2 * pi / 180);
    const T sqrt_hav_lon = std::sin((lon1 - lon2) / 2 * pi / 180);
    return 2 * r * std::asin(std::sqrt(sqrt_hav_lat * sqrt_hav_lat + std::cos(lat1 * pi / 180) * std::cos(lat2 * pi / 180) * sqrt_hav_lon * sqrt_hav_lon));
}

TropicalCyclones::TropicalCyclones(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p)
    : AgentImpact(std::move(base_forcing_p)), ProxiedImpact(impact_node["proxy"]), Impact(impact_node) {
    forcing_filename = impact_node["wind_speed"]["file"].as<std::string>();
    forcing_varname = impact_node["wind_speed"]["variable"].as<std::string>();

    year_from = impact_node["years"]["from"].as<int>();
    year_to = impact_node["years"]["to"].as<int>();
    if (year_from > year_to) {
        throw std::runtime_error("tropical_cyclones - years: 'from' value should be less than 'to' value");
    }

    events_varname = impact_node["events_count_variable"].as<std::string>("event_count");
    basin = impact_node["basin"].as<std::string>("");
    realization = impact_node["realization"].as<std::size_t>();
    threshold = impact_node["threshold"].as<float>();
    velocity = impact_node["velocity"].as<float>();

    random_generator.seed(impact_node["seed"].as<int>(0));

    read_sectors(impact_node);
    read_isoraster(impact_node["isoraster"], base_forcing.get_regions());

    for (const auto& season_node : impact_node["seasons"].as_map()) {
        auto from = season_node.second["from"].as<unsigned int>();
        auto to = season_node.second["to"].as<unsigned int>();
        if (from < 1 || from > 12 || to < 1 || to > 12) {
            throw std::runtime_error("Invalid season months given");
        }
        if (to < from) {
            to = cumulative_days_per_month[to] + 365;
        } else {
            to = cumulative_days_per_month[to];
        }
        from = cumulative_days_per_month[from - 1];
        seasons[season_node.first] = std::make_pair(from, to);
    }
}

void TropicalCyclones::join(Output& output, const TemplateFunction& template_func) {
    auto filename = fill_template(forcing_filename, template_func);
    netCDF::NcFile forcing_file;
    try {
        forcing_file.open(filename, netCDF::NcFile::read);
    } catch (netCDF::exceptions::NcException& e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
    const auto forcing_variable = forcing_file.getVar(forcing_varname);
    if (forcing_variable.isNull()) {
        throw std::runtime_error(filename + ": Variable '" + forcing_varname + "' not found");
    }
    if (!check_dimensions(forcing_variable, {"realization", "year", "event", "lat", "lon"})
        && !check_dimensions(forcing_variable, {"realization", "year", "event", "latitude", "longitude"})) {
        throw std::runtime_error(filename + " - " + forcing_varname + ": Unexpected dimensions");
    }
    const auto& dimensions = forcing_variable.getDims();
    const auto realization_count = dimensions[0].getSize();
    if (realization >= realization_count) {
        throw std::runtime_error(filename + ": Chosen realization not present");
    }
    // TimeVariable time_variable(forcing_file, filename, time_shift);
    GeoGrid<float> forcing_grid;
    forcing_grid.read_from_netcdf(forcing_file, filename);
    if (!isoraster_grid.is_compatible(forcing_grid)) {
        throw std::runtime_error(filename + ": Forcing and ISO raster not compatible in raster resolution");
    }

    read_proxy(fill_template(proxy_filename, template_func), output.get_regions());

    if (basin.empty()) {
        basin = template_func("basin", "basin");
    }

    const auto events_variable = forcing_file.getVar(events_varname);
    if (events_variable.isNull()) {
        throw std::runtime_error(filename + ": Variable '" + events_varname + "' not found");
    }
    if (!check_dimensions(events_variable, {"realization", "year"})) {
        throw std::runtime_error(filename + " - " + events_varname + ": Unexpected dimensions");
    }

    std::vector<int> years;
    {
        const auto years_varname = "year";
        const auto years_variable = forcing_file.getVar(years_varname);
        if (years_variable.isNull()) {
            throw std::runtime_error(filename + ": Variable '" + years_varname + "' not found");
        }
        if (!check_dimensions(years_variable, {"year"})) {
            throw std::runtime_error(filename + " - " + years_varname + ": Unexpected dimensions");
        }
        years.resize(years_variable.getDim(0).getSize());
        years_variable.getVar({0}, {years.size()}, &years[0]);
    }

    std::vector<ForcingType> chunk_buffer(chunk_size * forcing_grid.size());
    auto forcing_series = ForcingSeries<AgentForcing>(base_forcing, ReferenceTime(ReferenceTime::year(year_from), 24 * 60 * 60));
    std::vector<ForcingType> region_forcing(regions.size());

    progressbar::ProgressBar year_bar(year_to - year_from + 1, filename, true);
    for (int year = year_from; year <= year_to; ++year) {
        const auto year_it = std::find(std::begin(years), std::end(years), year);
        if (year_it == std::end(years)) {
            throw std::runtime_error(filename + ": Year " + std::to_string(year) + " not present");
        }
        const std::size_t year_index = year_it - std::begin(years);

        int events_cnt_read;
        events_variable.getVar({realization, year_index}, {1, 1}, &events_cnt_read);
        const std::size_t events_cnt = events_cnt_read;

        std::size_t chunk_pos = chunk_size;
        progressbar::ProgressBar event_bar(events_cnt, "Events", true);
        for (std::size_t event = 0; event < events_cnt; ++event) {
            if (chunk_pos == chunk_size) {
                forcing_variable.getVar(
                    {realization, year_index, event, 0, 0},
                    {1, 1, event + chunk_size > events_cnt ? events_cnt - event : chunk_size, forcing_grid.lat_count, forcing_grid.lon_count},
                    &chunk_buffer[0]);
                chunk_pos = 0;
            }
            nvector::View<ForcingType, 2> forcing_values(
                std::begin(chunk_buffer) + chunk_pos * forcing_grid.size(),
                {nvector::Slice{0, forcing_grid.lat_count, static_cast<int>(forcing_grid.lon_count)}, nvector::Slice{0, forcing_grid.lon_count, 1}});
            ++chunk_pos;
            std::fill(std::begin(region_forcing), std::end(region_forcing), 0);
            std::size_t lat_min = std::numeric_limits<std::size_t>::max();
            std::size_t lat_max = 0;
            std::size_t lon_min = std::numeric_limits<std::size_t>::max();
            std::size_t lon_max = 0;
            GeoGrid<float> common_grid;
            nvector::foreach_view(common_grid_view(common_grid, GridView<int>{isoraster, isoraster_grid}, GridView<ForcingType>{proxy_values, proxy_grid},
                                                   GridView<ForcingType>{forcing_values, forcing_grid}),
                                  [&](std::size_t lat_index, std::size_t lon_index, int i, ForcingType proxy_value, ForcingType forcing_v) {
                                      if (forcing_v > 1e10 || std::isnan(forcing_v)) {
                                          return true;
                                      }
                                      if (forcing_v >= threshold) {
                                          lat_min = std::min(lat_min, lat_index);
                                          lat_max = std::max(lat_max, lat_index);
                                          lon_min = std::min(lon_min, lon_index);
                                          lon_max = std::max(lon_max, lon_index);
                                          if (proxy_value <= 0 || i < 0 || std::isnan(proxy_value)) {
                                              return true;
                                          }
                                          region_forcing[i] += proxy_value;
                                      }
                                      return true;
                                  });
            AgentForcing forcing(base_forcing);
            for (std::size_t i = 0; i < regions.size(); ++i) {
                const auto region = regions[i];
                if (region < 0) {
                    continue;
                }
                const auto total_proxy_value = total_proxy[i];
                if (total_proxy_value <= 0) {
                    continue;
                }
                const auto r = region_forcing[i];
                for (const auto sector : sectors) {
                    forcing(sector, region) = (total_proxy_value - r) / total_proxy_value;
                }
            }
            const auto duration = static_cast<int>(
                std::ceil(distance(common_grid.lon(lon_min), common_grid.lat(lat_min), common_grid.lon(lon_min), common_grid.lat(lat_max)) / velocity / 24));
            const auto& season = seasons.at(basin);
            std::uniform_int_distribution<int> distribution(season.first, season.second - duration);
            const auto start = distribution(random_generator);
            const auto base_time = ReferenceTime::year(year);
            for (std::time_t t = start; t < start + duration; ++t) {
                forcing_series.insert_forcing(base_time + t * 24 * 60 * 60, forcing, ForcingCombination::ADD);
            }
            ++event_bar;
        }
        event_bar.close(true);
        ++year_bar;
    }
    output.include_forcing(forcing_series);
    year_bar.close(true);
}

}  // namespace impactgen
