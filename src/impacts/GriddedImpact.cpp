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

#include "impacts/GriddedImpact.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include "netcdftools.h"

namespace impactgen {

void GriddedImpact::read_isoraster(const settings::SettingsNode& isoraster_node, const std::unordered_map<std::string, std::size_t>& all_regions) {
    const auto& isoraster_filename = isoraster_node["file"].as<std::string>();
    const auto& isoraster_varname = isoraster_node["variable"].as<std::string>();
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
    isoraster_grid.read_from_netcdf(isoraster_file, isoraster_filename);
    isoraster.resize(-1, isoraster_grid.lat_count, isoraster_grid.lon_count);
    isoraster_variable.getVar({0, 0}, {isoraster_grid.lat_count, isoraster_grid.lon_count}, &isoraster.data()[0]);
    const auto& isoraster_index_varname = isoraster_node["index"].as<std::string>("index");
    const auto isoraster_regions_variable = isoraster_file.getVar(isoraster_index_varname);
    if (isoraster_regions_variable.isNull()) {
        throw std::runtime_error("Variable '" + isoraster_index_varname + "' not found in " + isoraster_filename);
    }
    if (!check_dimensions(isoraster_variable, {"lat", "lon"}) && !check_dimensions(isoraster_variable, {"latitude", "longitude"})) {
        throw std::runtime_error(isoraster_filename + " - " + isoraster_varname + ": Unexpected dimensions");
    }
    std::vector<char*> isoraster_regions(isoraster_regions_variable.getDim(0).getSize());
    isoraster_regions_variable.getVar({0}, {isoraster_regions.size()}, &isoraster_regions[0]);
    regions.reserve(isoraster_regions.size());
    bool verbose = isoraster_node["verbose"].as<bool>(false);
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

}  // namespace impactgen
