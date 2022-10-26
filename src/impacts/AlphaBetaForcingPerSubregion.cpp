/*
  Copyright (C) 2022 Sven Willner <sven.willner@pik-potsdam.de>
                     Kilian Kuhla <kilian.kuhla@pik-potsdam.de>
                     Lennart Quante <lennart.quante@pik-potsdam.de>

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

#include "impacts/AlphaBetaForcingPerSubregion.h"

#include <algorithm>
#include <cmath>
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

    AlphaBetaForcingPerSubregion::AlphaBetaForcingPerSubregion(const settings::SettingsNode &impact_node,
                                                               AgentForcing base_forcing_p)
            : AgentImpact(std::move(base_forcing_p)), ProxiedImpact(impact_node["proxy"]), Impact(impact_node) {
        forcing_filename = impact_node["forcing"]["file"].as<std::string>();
        forcing_varname = impact_node["forcing"]["variable"].as<std::string>();
        parameters = impact_node["parameters"];                           // load forcing parameters indexed by subnational regions
        parameters_raster_node = impact_node["parameters_raster"]; //raster of subnational regions
        read_isoraster(impact_node["isoraster"],
                       base_forcing.get_regions()); //raster of acclimate regions to apply forcing to
    }

    void AlphaBetaForcingPerSubregion::join(Output &output, const TemplateFunction &template_func) {
        auto filename = fill_template(forcing_filename, template_func);
        netCDF::NcFile forcing_file;
        try {
            forcing_file.open(filename, netCDF::NcFile::read);
        } catch (netCDF::exceptions::NcException &e) {
            throw std::runtime_error(filename + ": " + e.what());
        }
        const auto forcing_variable = forcing_file.getVar(forcing_varname);
        if (forcing_variable.isNull()) {
            throw std::runtime_error(filename + ": Variable '" + forcing_varname + "' not found");
        }
        if (!check_dimensions(forcing_variable, {"time", "lat", "lon"}) &&
            !check_dimensions(forcing_variable, {"time", "latitude", "longitude"})) {
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
        std::size_t chunk_pos = chunk_size;
        std::vector<ForcingType> chunk_buffer(chunk_size *forcing_grid.size());
        progressbar::ProgressBar time_bar(time_variable.times.size(), filename, true);
        std::vector<ForcingType> region_forcing(regions.size());

        struct SectorParameter {
            std::size_t sector_index;
            ForcingType value;
        };
        struct RegionParameters {
            std::vector<SectorParameter> slope;  // vector of forcing slopes per sector
            std::vector<SectorParameter> intercept; // vector of forcing intercepts per sector
        };

        std::unordered_map<std::string, std::size_t> parameters_regions_map;
        const auto &all_sectors = base_forcing.get_sectors();
        // generate sector vecotr with ALL sectors included. checking in forcing generation if forcing == 0 for sector
        for (const auto sector: all_sectors) {
            std::cout << sector.first << ": " << sector.second << std::endl;
            sectors.push_back(sector.second);
        }
        std::vector<RegionParameters> region_parameters;
        for (const auto &parameters_current_region: parameters.as_map()) {
            parameters_regions_map[parameters_current_region.first] = parameters_regions_map.size();
            RegionParameters parameters_struct;
            for (const auto &node: parameters_current_region.second["sector_slope"].as_map()) {
                parameters_struct.slope.push_back(
                        {all_sectors.at(node.first), node.second.as<ForcingType>()});
            }
            for (const auto &node: parameters_current_region.second["sector_intercept"].as_map()) {
                parameters_struct.intercept.push_back(
                        {all_sectors.at(node.first), node.second.as<ForcingType>()});
            }
            region_parameters.emplace_back(std::move(parameters_struct));
        }
        nvector::Vector<int, 2> parameters_isoraster;
        GeoGrid<float> parameters_isoraster_grid;
        std::vector<int> parameters_regions;
        read_isoraster(parameters_raster_node, parameters_regions_map, parameters_isoraster, parameters_isoraster_grid,
                       parameters_regions);

        if (!parameters_isoraster_grid.is_compatible(forcing_grid)) {
            throw std::runtime_error(
                    filename + ": Forcing and parameter ISO raster not compatible in raster resolution");
        }

        for (std::size_t t = 0; t < time_variable.times.size(); ++t) {
            if (chunk_pos == chunk_size) {
                forcing_variable.getVar(
                        {t, 0, 0},
                        {t + chunk_size > time_variable.times.size() ? time_variable.times.size() - t : chunk_size,
                         forcing_grid.lat_count, forcing_grid.lon_count},
                        &chunk_buffer[0]);
                chunk_pos = 0;
            }
            nvector::View<ForcingType, 2> forcing_values(
                    std::begin(chunk_buffer) + chunk_pos * forcing_grid.size(),
                    {nvector::Slice{0, forcing_grid.lat_count, static_cast<int>(forcing_grid.lon_count)},
                     nvector::Slice{0, forcing_grid.lon_count, 1}});
            ++chunk_pos;
            GeoGrid<float> common_grid;
            AgentForcing &forcing = forcing_series.insert_forcing(time_variable.times[t]);
            nvector::foreach_view(
                    common_grid_view(common_grid, GridView<int>{parameters_isoraster, parameters_isoraster_grid},
                                     GridView<int>{isoraster, isoraster_grid},
                                     GridView<ForcingType>{proxy_values, proxy_grid},
                                     GridView<ForcingType>{forcing_values, forcing_grid}),
                    [&](std::size_t lat_index, std::size_t lon_index, int parameters_i, int i, ForcingType proxy_value,
                        ForcingType forcing_v) {
                        (void) lat_index;
                        (void) lon_index;
                        if (forcing_v > 1e10 || proxy_value <= 0 || i < 0 || std::isnan(forcing_v) ||
                            std::isnan(proxy_value)) {
                            return true;
                        }
                        const auto region = regions[i];
                        const auto parameters_region = parameters_regions[parameters_i];
                        const RegionParameters &parameters_current_region = region_parameters[parameters_region];
                        if (region < 0) {
                            return true;
                        }
                        for (const auto &slope: parameters_current_region.slope) { //iterate over sectors using slope

                            ForcingType change_of_productivity = std::min(ForcingType(1.0),
                                                                          std::max(ForcingType(-1.0),
                                                                                   slope.value * forcing_v +
                                                                                   parameters_current_region.intercept[slope.sector_index].value));
                            forcing(slope.sector_index, region) -=
                                    change_of_productivity * proxy_value;
                        }
                        return true;
                    });


            for (std::size_t i = 0; i < regions.size(); ++i) {
                const auto region = regions[i];
                const auto total_proxy_value = total_proxy[i];

                if (total_proxy_value <= 0) {
                    continue;
                }

                if (region < 0) {
                    continue;
                }

                for (std::size_t s = 0; s < sectors.size(); ++s) {
                    if (std::isnan(forcing(sectors[s],
                                           region))) { // check if forcing calculated for this sector in this region
                        continue;
                    }
                    forcing(sectors[s], region) = (total_proxy_value - forcing(sectors[s], region)) / total_proxy_value;
                    forcing(sectors[s], region) = std::min(ForcingType(2.0), std::max(ForcingType(0.0),
                                                                                      forcing(sectors[s],
                                                                                              region))); //upper and lower bound for forcing
                }
            }
            ++time_bar;
        }
        output.include_forcing(forcing_series);
        time_bar.close(true);
    }

}  // namespace impactgen
