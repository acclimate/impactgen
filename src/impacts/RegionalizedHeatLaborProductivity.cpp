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

#include "impacts/RegionalizedHeatLaborProductivity.h"
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

    RegionalizedHeatLaborProductivity::RegionalizedHeatLaborProductivity(const settings::SettingsNode &impact_node,
                                                                         AgentForcing base_forcing_p)
            : AgentImpact(std::move(base_forcing_p)), ProxiedImpact(impact_node["proxy"]), Impact(impact_node) {
        forcing_filename = impact_node["day_temperature"]["file"].as<std::string>();
        forcing_varname = impact_node["day_temperature"]["variable"].as<std::string>();
        unit = impact_node["day_temperature"]["unit"].as<std::string>(); //load unit to convert to degree C; K= degree Kelvin, C= degree Celsius
        parameters = impact_node["parameters"]; //load forcing parameters indexed by region
        const auto &all_sectors = base_forcing.get_sectors();
        for (const auto node: impact_node["sectors"].as_map()) {
            sectors.push_back(all_sectors.at(node.first));
            intense_work.push_back(
                    node.second.as<bool>()); //parameter for sector: TRUE=> intense (outdoor work in shadow) or FALSE=> normal (indoor work)
        }

        read_isoraster(impact_node["isoraster"], base_forcing.get_regions());
    }

    void RegionalizedHeatLaborProductivity::join(Output &output, const TemplateFunction &template_func) {
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

        struct RegionParameters {
            ForcingType t_optimal;
            ForcingType first_order_coefficient;
            ForcingType second_order_coefficient;
            ForcingType intense_t_optimal;
            ForcingType intense_first_order_coefficient;
            ForcingType intense_second_order_coefficient;
        };

        const auto &regions_map = base_forcing.get_regions();
        std::vector<RegionParameters> region_parameters(regions_map.size());
        for (const auto &region: regions_map) {
            const auto &region_name = region.first;
            const auto region_index = region.second;
            const settings::SettingsNode &parameters_current_region = parameters[region_name];
            RegionParameters &parameters_struct = region_parameters[region_index];
            parameters_struct.t_optimal = parameters_current_region["T_optimal"].as<ForcingType>();
            parameters_struct.first_order_coefficient = parameters_current_region["first_order"].as<ForcingType>();
            parameters_struct.second_order_coefficient = parameters_current_region["second_order"].as<ForcingType>();
            parameters_struct.intense_t_optimal = parameters_current_region["T_optimal_intense"].as<ForcingType>();
            parameters_struct.intense_first_order_coefficient = parameters_current_region["first_order_intense"].as<ForcingType>();
            parameters_struct.intense_second_order_coefficient = parameters_current_region["second_order_intense"].as<ForcingType>();
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
            nvector::foreach_view(common_grid_view(common_grid, GridView<int>{isoraster, isoraster_grid},
                                                   GridView<ForcingType>{proxy_values, proxy_grid},
                                                   GridView<ForcingType>{forcing_values, forcing_grid}),
                                  [&](std::size_t lat_index, std::size_t lon_index, int i, ForcingType proxy_value,
                                      ForcingType forcing_v) {
                                      (void) lat_index;
                                      (void) lon_index;
                                      if (forcing_v > 1e10 || proxy_value <= 0 || i < 0 || std::isnan(forcing_v) ||
                                          std::isnan(proxy_value)) {
                                          return true;
                                      }

                                      const auto region = regions[i];

                                      if (region < 0) {
                                          return true;
                                      }
                                      const RegionParameters &parameters_current_region = region_parameters[region];

                                      ForcingType t_opt = parameters_current_region.t_optimal;
                                      ForcingType first_order_coefficient = parameters_current_region.first_order_coefficient;
                                      ForcingType second_order_coefficient = parameters_current_region.second_order_coefficient;

                                      for (std::size_t s = 0; s < sectors.size(); ++s) {
                                          if (intense_work[s]) {
                                              t_opt = parameters_current_region.intense_t_optimal;
                                              first_order_coefficient = parameters_current_region.intense_first_order_coefficient;
                                              second_order_coefficient = parameters_current_region.intense_second_order_coefficient;
                                          }

                                          const ForcingType scale_by_max = expf(first_order_coefficient * t_opt + second_order_coefficient * t_opt * t_opt);

                                          // calculate localized forcing as log of labor supply <= total productivity loss, i.e need to exponentiate
                                          //unit conversion if forcing_v in degree K:
                                          ForcingType ln_labor_supply;
                                          if (unit == "C") { //TODO: nicer way of unit checking
                                              ln_labor_supply = (first_order_coefficient * forcing_v +
                                                                 second_order_coefficient * forcing_v * forcing_v);
                                          } else {
                                              ForcingType kelvin_to_celsius = 273.15;
                                              ln_labor_supply =
                                                      (first_order_coefficient * (forcing_v - kelvin_to_celsius) +
                                                       second_order_coefficient * (forcing_v - kelvin_to_celsius) *
                                                       (forcing_v - kelvin_to_celsius));
                                          }

                                          ForcingType labor_supply = expf(ln_labor_supply)/scale_by_max;
                                          ForcingType reduction_labour_supply = std::max(ForcingType(0.0),1-labor_supply); //assumption: positive shock of labour supply excluded due to contracted working hours

                                          forcing(sectors[s], region) += std::min(ForcingType(1.0), reduction_labour_supply) *
                                                                         proxy_value;
                                      }
                                      return true;
                                  });

            for (std::size_t i = 0; i < regions.size(); ++i) {
                const auto region = regions[i];
                if (region < 0) {
                    continue;
                }
                const auto total_proxy_value = total_proxy[i];
                if (total_proxy_value <= 0) {
                    continue;
                }
                for (const auto sector: sectors) {
                    forcing(sector, region) = (total_proxy_value - forcing(sector, region)) / total_proxy_value;
                }
            }
            ++time_bar;
        }
        output.include_forcing(forcing_series);
        time_bar.close(true);
    }


}  // namespace impactgen
