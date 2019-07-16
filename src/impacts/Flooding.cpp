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
#include "GeoGrid.h"
#include "Output.h"
#include "TimeVariable.h"
#include "helpers.h"
#include "netcdftools.h"
#include "nvector.h"
#include "progressbar.h"

namespace impactgen {

Flooding::Flooding(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p)
    : AgentImpact(std::move(base_forcing_p)), ProxiedImpact(impact_node["proxy"]), Impact(impact_node) {
    forcing_filename = impact_node["flood_fraction"]["file"].as<std::string>();
    forcing_varname = impact_node["flood_fraction"]["variable"].as<std::string>();
    if (impact_node.has("recovery")) {
        recovery_exponent = impact_node["recovery"]["exponent"].as<ForcingType>();
        recovery_threshold = impact_node["recovery"]["threshold"].as<ForcingType>();
    } else {
        recovery_exponent = 0;
        recovery_threshold = 0;
    }

    read_sectors(impact_node);
    read_isoraster(impact_node["isoraster"], base_forcing.get_regions());
}

void Flooding::join(Output& output, const TemplateFunction& template_func) {
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
    if (!check_dimensions(forcing_variable, {"time", "lat", "lon"}) && !check_dimensions(forcing_variable, {"time", "latitude", "longitude"})) {
        throw std::runtime_error(filename + " - " + forcing_varname + ": Unexpected dimensions");
    }
    TimeVariable time_variable(forcing_file, filename, time_shift);
    GeoGrid<float> forcing_grid;
    forcing_grid.read_from_netcdf(forcing_file, filename);
    if (!isoraster_grid.is_compatible(forcing_grid)) {
        throw std::runtime_error(filename + ": Forcing and ISO raster not compatible in raster resolution");
    }

    read_proxy(fill_template(proxy_filename, template_func), output.get_regions());

    auto forcing_series = ForcingSeries<AgentForcing>(base_forcing, output.ref());
    if (last.data().empty()) {
        last.resize(0, forcing_grid.lat_count, forcing_grid.lon_count);
    } else if (!forcing_grid.is_compatible(last_grid) || forcing_grid.lat_count != last_grid.lat_count || forcing_grid.lon_count != last_grid.lon_count) {
        throw std::runtime_error(filename + ": Incompatible grids");
    }
    std::size_t chunk_pos = chunk_size;
    std::vector<ForcingType> chunk_buffer(chunk_size * forcing_grid.size());
    progressbar::ProgressBar time_bar(time_variable.times.size(), filename, true);
    std::vector<ForcingType> region_forcing(regions.size());
    for (std::size_t t = 0; t < time_variable.times.size(); ++t) {
        if (chunk_pos == chunk_size) {
            forcing_variable.getVar({t, 0, 0}, {std::min(chunk_size, time_variable.times.size() - t), forcing_grid.lat_count, forcing_grid.lon_count},
                                    &chunk_buffer[0]);
            chunk_pos = 0;
            time_bar.reset_eta();
        }
        nvector::View<ForcingType, 2> forcing_values(std::begin(chunk_buffer) + chunk_pos * forcing_grid.size(), forcing_grid.lat_count,
                                                     forcing_grid.lon_count);
        ++chunk_pos;
        std::fill(std::begin(region_forcing), std::end(region_forcing), 0);
        GeoGrid<float> common_grid;
        nvector::foreach_view(common_grid_view(common_grid, GridView<int>{isoraster, isoraster_grid}, GridView<ForcingType>{proxy_values, proxy_grid},
                                               GridView<ForcingType>{forcing_values, forcing_grid}, GridView<ForcingType>{last, forcing_grid}),
                              [&](std::size_t lat_index, std::size_t lon_index, int i, ForcingType proxy_value, ForcingType forcing_v, ForcingType& last_v) {
                                  (void)lat_index;
                                  (void)lon_index;
                                  if (forcing_v > 1e10 || proxy_value <= 0 || i < 0 || std::isnan(forcing_v) || std::isnan(proxy_value)) {
                                      return true;
                                  }
                                  auto rec = recovery_exponent * last_v;
                                  if (rec < recovery_threshold || rec > 1e10 || std::isnan(rec)) {
                                      rec = 0;
                                  }
                                  const auto v = std::min(forcing_v + rec, ForcingType(1.0));
                                  region_forcing[i] += v * proxy_value;
                                  last_v = v;
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
                forcing(sector, region) = (total_proxy_value - r) / total_proxy_value;
            }
        }
        ++time_bar;
    }
    output.include_forcing(forcing_series);
    time_bar.close(true);
    last_grid = forcing_grid;
}

}  // namespace impactgen
