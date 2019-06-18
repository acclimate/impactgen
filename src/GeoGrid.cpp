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

GeoGrid::GeoGrid(const netCDF::NcFile& file) : x(*this), y(*this) {
    {
        netCDF::NcVar x_var = file.getVar("x");
        if (x_var.isNull()) {
            x_var = file.getVar("lon");
            if (x_var.isNull()) {
                x_var = file.getVar("longitude");
                if (x_var.isNull()) {
                    throw std::runtime_error("No longitude variable found");
                }
            }
        }
        x_var.getVar({0}, &x_min);
        x_count = x_var.getDim(0).getSize();
        x_var.getVar({x_count - 1}, &x_max);
        float tmp;
        x_var.getVar({1}, &tmp);
        x_gridsize = tmp - x_min;
        t_x_min = std::min(x_min, x_max);
        t_x_max = std::max(x_min, x_max);
        t_x_gridsize = std::abs(x_gridsize);
    }
    {
        netCDF::NcVar y_var = file.getVar("y");
        if (y_var.isNull()) {
            y_var = file.getVar("lat");
            if (y_var.isNull()) {
                y_var = file.getVar("latitude");
                if (y_var.isNull()) {
                    throw std::runtime_error("No latitude variable found");
                }
            }
        }
        y_var.getVar({0}, &y_min);
        y_count = y_var.getDim(0).getSize();
        y_var.getVar({y_count - 1}, &y_max);
        float tmp;
        y_var.getVar({1}, &tmp);
        y_gridsize = tmp - y_min;
        t_y_min = std::min(y_min, y_max);
        t_y_max = std::max(y_min, y_max);
        t_y_gridsize = std::abs(y_gridsize);
    }
}

float GeoGrid::operator/(const GeoGrid& other) const { return t_x_gridsize * t_y_gridsize / other.abs_x_gridsize() / other.abs_y_gridsize(); }

bool GeoGrid::is_compatible(const GeoGrid& other) const {
    return std::abs(t_x_gridsize - other.abs_x_gridsize()) < 1e-5 && std::abs(t_y_gridsize - other.abs_y_gridsize()) < 1e-5;
}

inline unsigned int GeoGrid::x_index(float x_var) const {
    if (x_min < x_max) {
        if (x_var < x_min || x_var > x_max + x_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    } else {
        if (x_var > x_min || x_var < x_max + x_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    }
    return static_cast<unsigned int>((x_var - x_min) * x_count / (x_max - x_min + x_gridsize));
}

inline unsigned int GeoGrid::y_index(float y_var) const {
    if (y_min < y_max) {
        if (y_var < y_min || y_var > y_max + y_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    } else {
        if (y_var > y_min || y_var < y_max + y_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    }
    return static_cast<unsigned int>((y_var - y_min) * y_count / (y_max - y_min + y_gridsize));
}

template<typename T>
T GeoGrid::read(float x_var, float y_var, const std::vector<T>& data) const {
    const unsigned int x_i = x_index(x_var);
    if (std::isnan(x_i)) {
        return std::numeric_limits<T>::quiet_NaN();
    }
    const unsigned int y_i = y_index(y_var);
    if (std::isnan(y_i)) {
        return std::numeric_limits<T>::quiet_NaN();
    }
    T res = data[y_i * x_count + x_i];
    if (res > 1e18) {
        res = std::numeric_limits<T>::quiet_NaN();
    }
    return res;
}

template<typename T>
void GeoGrid::write(float x_var, float y_var, std::vector<T>& data, T value) const {
    const unsigned int x_i = x_index(x_var);
    if (std::isnan(x_i)) {
        return;
    }
    const unsigned int y_i = y_index(y_var);
    if (std::isnan(y_i)) {
        return;
    }
    data[y_i * x_count + x_i] = value;
}

template float GeoGrid::read(float x_var, float y_var, const std::vector<float>& data) const;
template double GeoGrid::read(float x_var, float y_var, const std::vector<double>& data) const;
template int GeoGrid::read(float x_var, float y_var, const std::vector<int>& data) const;
template void GeoGrid::write(float x_var, float y_var, std::vector<float>& data, float value) const;
template void GeoGrid::write(float x_var, float y_var, std::vector<double>& data, double value) const;
template void GeoGrid::write(float x_var, float y_var, std::vector<int>& data, int value) const;

}  // namespace impactgen
