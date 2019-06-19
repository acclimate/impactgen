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

#include "impacts/Flooding.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include "Output.h"
#include "TimeVariable.h"
#include "helpers.h"
#include "netcdf_headers.h"
#include "nvector.h"
#include "progressbar.h"

namespace impactgen {

Flooding::Flooding(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p) : base_forcing(std::move(base_forcing_p)) {
    time_shift = impact_node["time_shift"].as<int>(0);
    chunk_size = impact_node["chunk_size"].as<std::size_t>(10);
    verbose = impact_node["verbose"].as<bool>(false);
    forcing_filename = impact_node["flood_fraction"]["file"].as<std::string>();
    forcing_varname = impact_node["flood_fraction"]["variable"].as<std::string>();
    proxy_filename = impact_node["proxy"]["file"].as<std::string>();
    proxy_varname = impact_node["proxy"]["variable"].as<std::string>();
    if (impact_node.has("recovery")) {
        recovery_exponent = impact_node["recovery"]["exponent"].as<ForcingType>();
        recovery_threshold = impact_node["recovery"]["threshold"].as<ForcingType>();
    } else {
        recovery_exponent = 0;
        recovery_threshold = 0;
    }

    const auto& all_sectors = base_forcing.get_sectors();
    if (impact_node.has("sectors")) {
        auto seq = impact_node["sectors"].as_sequence();
        std::transform(std::begin(seq), std::end(seq), std::back_inserter(sectors),
                       [&](const settings::SettingsNode& s) -> int { return all_sectors.at(s.as<std::string>()); });
    } else {
        sectors.resize(all_sectors.size());
        std::transform(std::begin(all_sectors), std::end(all_sectors), std::begin(sectors),
                       [&](const std::pair<std::string, int>& s) -> int { return s.second; });
    }

    const auto& isoraster_filename = impact_node["isoraster"]["file"].as<std::string>();
    const auto& isoraster_varname = impact_node["isoraster"]["variable"].as<std::string>();
    netCDF::NcFile isoraster_file;
    try {
        isoraster_file.open(isoraster_filename, netCDF::NcFile::read);
    } catch (netCDF::exceptions::NcException& e) {
        throw std::runtime_error(isoraster_filename + ": " + e.what());
    }
    const auto isoraster_variable = isoraster_file.getVar(isoraster_varname);
    if (isoraster_variable.isNull()) {
        throw std::runtime_error("Variable '" + isoraster_varname + "' not found in " + isoraster_filename);
    }
    isoraster_grid = std::make_unique<GeoGrid>(isoraster_file);
    isoraster.resize(-1, isoraster_grid->y_size(), isoraster_grid->x_size());
    isoraster_variable.getVar({0, 0}, {isoraster_grid->y_size(), isoraster_grid->x_size()}, &isoraster.data()[0]);
    const auto isoraster_regions_variable = isoraster_file.getVar("index");
    if (isoraster_regions_variable.isNull()) {
        throw std::runtime_error("Variable 'index' not found in " + isoraster_filename);
    }
    // TODO check dimensions
    std::vector<char*> isoraster_regions(isoraster_regions_variable.getDim(0).getSize());
    isoraster_regions_variable.getVar({0}, {isoraster_regions.size()}, &isoraster_regions[0]);
    const auto& all_regions = base_forcing.get_regions();
    regions.reserve(isoraster_regions.size());
    for (const auto& region_name : isoraster_regions) {
        const auto& region = all_regions.find(region_name);
        if (region == std::end(all_regions)) {
            if (verbose) {
                std::cerr << "Warning: ISO-Raster region " << region_name << " ignored\n";
            }
            regions.push_back(-1);
        } else {
            regions.push_back(region->second);
        }
    }
}

void Flooding::join(Output& output, const TemplateFunction& template_func) {
    const auto filename = fill_template(forcing_filename, template_func);
    netCDF::NcFile forcing_file;
    try {
        forcing_file.open(filename, netCDF::NcFile::read);
    } catch (netCDF::exceptions::NcException& e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
    const auto forcing_variable = forcing_file.getVar(forcing_varname);
    if (forcing_variable.isNull()) {
        throw std::runtime_error("Variable '" + forcing_varname + "' not found in " + forcing_filename);
    }
    // TODO check dimensions
    TimeVariable time_variable(forcing_file, time_shift);
    GeoGrid forcing_grid(forcing_file);
    if (!isoraster_grid->is_compatible(forcing_grid)) {
        throw std::runtime_error("Forcing and ISO raster not compatible in raster resolution");
    }

    netCDF::NcFile proxy_file;
    try {
        proxy_file.open(fill_template(proxy_filename, template_func), netCDF::NcFile::read);
    } catch (netCDF::exceptions::NcException& e) {
        throw std::runtime_error(proxy_filename + ": " + e.what());
    }
    const auto proxy_variable = proxy_file.getVar(proxy_varname);
    if (proxy_variable.isNull()) {
        throw std::runtime_error("Variable '" + proxy_varname + "' not found in " + proxy_filename);
    }
    // TODO check dimensions
    GeoGrid proxy_grid(proxy_file);
    if (!proxy_grid.is_compatible(forcing_grid)) {
        throw std::runtime_error("Forcing and proxy not compatible in raster resolution");
    }
    nvector::Vector<ForcingType, 2> proxy_values(0, proxy_grid.y_size(), proxy_grid.x_size());
    proxy_variable.getVar({0, 0}, {proxy_grid.y_size(), proxy_grid.x_size()}, &proxy_values.data()[0]);

    std::vector<ForcingType> total_proxy(regions.size(), 0);
    {
        ForcingType total_proxy_sum = 0.0;
        ForcingType total_proxy_sum_all = 0.0;

        nvector::foreach_view(std::make_tuple(isoraster, proxy_values),
                              [&](std::size_t lat, std::size_t lon, int i, ForcingType v) {  // TODO account for shifted grids
                                  (void)lat;
                                  (void)lon;
                                  if (v <= 0 || std::isnan(v)) {
                                      return true;
                                  }
                                  total_proxy_sum_all += v;
                                  if (i < 0) {
                                      return true;
                                  }
                                  total_proxy[i] += v;
                                  total_proxy_sum += v;
                                  return true;
                              });
        if (verbose) {
            std::cout << "Total proxy sum: " << total_proxy_sum << " (" << total_proxy_sum_all << ")" << std::endl;
            for (std::size_t i = 0; i < regions.size(); ++i) {
                const auto region = regions[i];
                if (region < 0) {
                    continue;
                }
                const auto total_proxy_value = total_proxy[i];
                if (total_proxy_value <= 0) {
                    std::cerr << "Warning: " << output.get_regions()[region] << " has zero proxy" << std::endl;
                }
            }
        }
    }

    auto forcing_series = ForcingSeries<AgentForcing>(base_forcing, time_variable.ref());
    nvector::Vector<ForcingType, 2> current(0, forcing_grid.y_size(), forcing_grid.x_size());
    if (last.data().size() == 0) {
        last.resize(0, forcing_grid.y_size(), forcing_grid.x_size());
    } else if (last.data().size() != current.data().size()) {  // TODO
        throw std::runtime_error("Incompatible grid sizes");
    }
    // TODO check actual GeoGrids
    std::size_t chunk_pos = chunk_size;
    std::vector<ForcingType> chunk_buffer(chunk_size * forcing_grid.size());
    progressbar::ProgressBar time_bar(time_variable.times.size(), filename);
    std::vector<ForcingType> region_forcing(regions.size());
    for (std::size_t t = 0; t < time_variable.times.size(); ++t) {
        if (chunk_pos == chunk_size) {
            forcing_variable.getVar(
                {t, 0, 0},
                {t > time_variable.times.size() - chunk_size ? time_variable.times.size() - t : chunk_size, forcing_grid.y_size(), forcing_grid.x_size()},
                &chunk_buffer[0]);
            chunk_pos = 0;
        }
        std::copy_n(std::begin(chunk_buffer) + chunk_pos * forcing_grid.size(), forcing_grid.size(), std::begin(current.data()));
        ++chunk_pos;
        std::fill(std::begin(region_forcing), std::end(region_forcing), 0);
        nvector::foreach_view(std::make_tuple(isoraster, proxy_values, current),  // TODO account for shifted grids
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
}

}  // namespace impactgen
