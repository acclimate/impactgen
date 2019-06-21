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

#include "impacts/ProxiedImpact.h"
#include <cmath>
#include <string>
#include "GeoGrid.h"
#include "settingsnode.h"

namespace impactgen {

ProxiedImpact::ProxiedImpact(const settings::SettingsNode& proxy_node) : GriddedImpact() {
    proxy_filename = proxy_node["file"].as<std::string>();
    proxy_varname = proxy_node["variable"].as<std::string>();
    verbose = proxy_node["verbose"].as<bool>(false);
}

void ProxiedImpact::read_proxy(const std::string& filename, const std::vector<std::string>& all_regions) {
    netCDF::NcFile proxy_file;
    try {
        proxy_file.open(filename, netCDF::NcFile::read);
    } catch (netCDF::exceptions::NcException& e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
    const auto proxy_variable = proxy_file.getVar(proxy_varname);
    if (proxy_variable.isNull()) {
        throw std::runtime_error(filename + ": Variable '" + proxy_varname + "' not found");
    }
    if (!check_dimensions(proxy_variable, {"lat", "lon"}) && !check_dimensions(proxy_variable, {"latitude", "longitude"})) {
        throw std::runtime_error(filename + " - " + proxy_varname + ": Unexpected dimensions");
    }
    proxy_grid.read_from_netcdf(proxy_file, filename);
    proxy_values.resize(0, proxy_grid.lat_count, proxy_grid.lon_count);
    proxy_variable.getVar({0, 0}, {proxy_grid.lat_count, proxy_grid.lon_count}, &proxy_values.data()[0]);
    if (!proxy_grid.is_compatible(isoraster_grid)) {
        throw std::runtime_error("Forcing and proxy not compatible in raster resolution");
    }

    total_proxy.resize(regions.size(), 0);
    {
        ForcingType total_proxy_sum = 0.0;
        ForcingType total_proxy_sum_all = 0.0;

        nvector::foreach_view(common_grid_box(GridView<int>{isoraster, isoraster_grid}, GridView<ForcingType>{proxy_values, proxy_grid}),
                              [&](std::size_t lat_index, std::size_t lon_index, int i, ForcingType v) {
                                  (void)lat_index;
                                  (void)lon_index;
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
                    std::cerr << "Warning: " << all_regions[region] << " has zero proxy" << std::endl;
                }
            }
        }
    }
}

}  // namespace impactgen
