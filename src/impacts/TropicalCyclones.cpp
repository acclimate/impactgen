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
#include <stdexcept>
#include <string>
#include "Output.h"
#include "TimeVariable.h"
#include "helpers.h"
#include "netcdftools.h"
#include "nvector.h"
#include "progressbar.h"

namespace impactgen {

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

    read_sectors(impact_node);
    read_isoraster(impact_node["isoraster"], base_forcing.get_regions());
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
    // TimeVariable time_variable(forcing_file, filename, time_shift);
    GeoGrid<float> forcing_grid;
    forcing_grid.read_from_netcdf(forcing_file, filename);
    if (!isoraster_grid.is_compatible(forcing_grid)) {
        throw std::runtime_error("Forcing and ISO raster not compatible in raster resolution");
    }

    read_proxy(fill_template(proxy_filename, template_func), output.get_regions());

    /*
    auto forcing_series = ForcingSeries<AgentForcing>(base_forcing, time_variable.ref());
    nvector::Vector<ForcingType, 2> current(0, forcing_grid.lat_count, forcing_grid.lon_count);
    std::size_t chunk_pos = chunk_size;
    std::vector<ForcingType> chunk_buffer(chunk_size * forcing_grid.size());
    progressbar::ProgressBar time_bar(time_variable.times.size(), filename, true);
    std::vector<ForcingType> region_forcing(regions.size());
    for (std::size_t t = 0; t < time_variable.times.size(); ++t) {
        if (chunk_pos == chunk_size) {
            forcing_variable.getVar(
                {t, 0, 0},
                {t > time_variable.times.size() - chunk_size ? time_variable.times.size() - t : chunk_size, forcing_grid.lat_count, forcing_grid.lon_count},
                &chunk_buffer[0]);
            chunk_pos = 0;
        }
        std::copy_n(std::begin(chunk_buffer) + chunk_pos * forcing_grid.size(), forcing_grid.size(), std::begin(current.data()));
        ++chunk_pos;
        std::fill(std::begin(region_forcing), std::end(region_forcing), 0);
        nvector::foreach_view(common_grid_box(GridView<int>(isoraster, isoraster_grid), GridView<ForcingType>(proxy_values, proxy_grid)),
                              [&](std::size_t lat, std::size_t lon, int i, ForcingType proxy_value, ForcingType& forcing_v) {
                                  if (forcing_v > 1e10 || proxy_value <= 0 || i < 0 || std::isnan(forcing_v) || std::isnan(proxy_value)) {
                                      return true;
                                  }
                                  auto rec = recovery_exponent * last(lat, lon);
                                  if (rec < recovery_threshold || rec > 1e10 || std::isnan(rec)) {
                                      rec = 0;
                                  }
                                  const auto v = std::min(forcing_v + rec, ForcingType(1.0));
                                  region_forcing[i] += (1 - v) * proxy_value;
                                  last(lat, lon) = v;
                                  return true;
                              });
        AgentForcing& forcing = forcing_series.insert_forcing(time_variable.times[t]);
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
                forcing(sector, region) = r / total_proxy_value;
            }
        }
        ++time_bar;
    }
    output.include_forcing(forcing_series);
    time_bar.close(true);
    */

    nvector::Vector<ForcingType, 2> current(0, forcing_grid.lat_count, forcing_grid.lon_count);
    const auto views = common_grid_box(GridView<int>{isoraster, isoraster_grid}, GridView<ForcingType>{proxy_values, proxy_grid},
                                       GridView<ForcingType>{current, forcing_grid});
}

}  // namespace impactgen
