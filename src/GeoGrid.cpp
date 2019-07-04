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

#include "GeoGrid.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace impactgen {

template<typename T>
void GeoGrid<T>::read_from_netcdf(const netCDF::NcFile& file, const std::string& filename) {
    {
        auto y_var = file.getVar("y");
        if (y_var.isNull()) {
            y_var = file.getVar("lat");
            if (y_var.isNull()) {
                y_var = file.getVar("latitude");
                if (y_var.isNull()) {
                    throw std::runtime_error(filename + ": No latitude variable found");
                }
            }
        }
        lat_count = y_var.getDim(0).getSize();
        if (lat_count < 2) {
            throw std::runtime_error(filename + ": Too few latitude values");
        }
        std::vector<T> values(lat_count);
        y_var.getVar(&values[0]);
        const auto lat_start = values[0];
        const auto lat_stop = values[lat_count - 1];
        lat_stepsize = values[1] - lat_start;
        for (std::size_t i = 2; i < lat_count; ++i) {
            if (std::abs((values[i] - values[i - 1] - lat_stepsize) / lat_stepsize) > 1e-2) {
                throw std::runtime_error(filename + ": No gaps in latitude values supported");
            }
        }
        lat_min = std::min(lat_start, lat_stop);
        lat_max = std::max(lat_start, lat_stop);
        lat_abs_stepsize = std::abs(lat_stepsize);
    }
    {
        auto x_var = file.getVar("x");
        if (x_var.isNull()) {
            x_var = file.getVar("lon");
            if (x_var.isNull()) {
                x_var = file.getVar("longitude");
                if (x_var.isNull()) {
                    throw std::runtime_error(filename + ": No longitude variable found");
                }
            }
        }
        lon_count = x_var.getDim(0).getSize();
        if (lon_count < 2) {
            throw std::runtime_error(filename + ": Too few longitude values");
        }
        std::vector<T> values(lon_count);
        x_var.getVar(&values[0]);
        const auto lon_start = values[0];
        const auto lon_stop = values[lon_count - 1];
        lon_stepsize = values[1] - lon_start;
        for (std::size_t i = 2; i < lon_count; ++i) {
            if (std::abs((values[i] - values[i - 1] - lon_stepsize) / lon_stepsize) > 1e-2) {
                throw std::runtime_error(filename + ": No gaps in longitude values supported");
            }
        }
        lon_min = std::min(lon_start, lon_stop);
        lon_max = std::max(lon_start, lon_stop);
        lon_abs_stepsize = std::abs(lon_stepsize);
    }
}

template struct GeoGrid<double>;
template struct GeoGrid<float>;
}  // namespace impactgen
