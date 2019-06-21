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
        for (int i = 2; i < lat_count; ++i) {
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
        for (int i = 2; i < lon_count; ++i) {
            if (std::abs((values[i] - values[i - 1] - lon_stepsize) / lon_stepsize) > 1e-2) {
                throw std::runtime_error(filename + ": No gaps in longitude values supported");
            }
        }
        lon_min = std::min(lon_start, lon_stop);
        lon_max = std::max(lon_start, lon_stop);
        lon_abs_stepsize = std::abs(lon_stepsize);
    }
}

template<typename T>
T GeoGrid<T>::operator/(const GeoGrid<T>& other) const {
    return lat_abs_stepsize * lon_abs_stepsize / other.lat_abs_stepsize / other.lon_abs_stepsize;
}

template<typename T>
bool GeoGrid<T>::is_compatible(const GeoGrid<T>& other) const {
    return std::abs(lat_abs_stepsize - other.lat_abs_stepsize) / lat_abs_stepsize < 1e-2
           && std::abs(lon_abs_stepsize - other.lon_abs_stepsize) / lon_abs_stepsize < 1e-2;
}

template<typename T>
std::size_t GeoGrid<T>::lat_index(T lat) const {
    const auto res = (lat - lat_min) * lat_count / (lat_max - lat_min + lat_stepsize);
    if (res < 0 || res >= lat_count) {
        return std::numeric_limits<std::size_t>::quiet_NaN();
    }
    return static_cast<std::size_t>(res);
}

template<typename T>
std::size_t GeoGrid<T>::lon_index(T lon) const {
    const auto res = (lon - lon_min) * lon_count / (lon_max - lon_min + lon_stepsize);
    if (res < 0 || res >= lon_count) {
        return std::numeric_limits<std::size_t>::quiet_NaN();
    }
    return static_cast<std::size_t>(res);
}

template<typename T>
template<typename V>
nvector::View<V, 2> GeoGrid<T>::box(const nvector::View<V, 2>& view, T lat_min_p, T lat_max_p, T lon_min_p, T lon_max_p) const {
    const auto& lat_slice = view.template slice<0>();
    const auto& lon_slice = view.template slice<1>();
    nvector::Slice new_lat_slice;
    nvector::Slice new_lon_slice;

    const auto lat_min_index = lat_index(lat_min_p);
    const auto lat_max_index = lat_index(lat_max_p);
    if (lat_min_index > lat_max_index) {
        new_lat_slice.begin = lat_slice.begin + lat_max_index * lat_slice.stride;
        new_lat_slice.size = lat_min_index - lat_max_index;
        new_lat_slice.stride = lat_slice.stride;
    } else {
        new_lat_slice.begin = lat_slice.begin + lat_min_index * lat_slice.stride;
        new_lat_slice.size = lat_max_index - lat_min_index;
        new_lat_slice.stride = lat_slice.stride;
    }

    const auto lon_min_index = lon_index(lon_min_p);
    const auto lon_max_index = lon_index(lon_max_p);
    if (lon_min_index > lon_max_index) {
        new_lon_slice.begin = lon_slice.begin + lon_max_index * lon_slice.stride;
        new_lon_slice.size = lon_min_index - lon_max_index;
        new_lon_slice.stride = lon_slice.stride;
    } else {
        new_lon_slice.begin = lon_slice.begin + lon_min_index * lon_slice.stride;
        new_lon_slice.size = lon_max_index - lon_min_index;
        new_lon_slice.stride = lon_slice.stride;
    }

    return typename nvector::View<V, 2>(view.data(), {new_lat_slice, new_lon_slice});
}

template class GeoGrid<double>;
template class GeoGrid<float>;
template nvector::View<double, 2> GeoGrid<double>::box(
    const nvector::View<double, 2>& view, double lat_min_p, double lat_max_p, double lon_min_p, double lon_max_p) const;
template nvector::View<double, 2> GeoGrid<float>::box(
    const nvector::View<double, 2>& view, float lat_min_p, float lat_max_p, float lon_min_p, float lon_max_p) const;
template nvector::View<float, 2> GeoGrid<double>::box(
    const nvector::View<float, 2>& view, double lat_min_p, double lat_max_p, double lon_min_p, double lon_max_p) const;
template nvector::View<float, 2> GeoGrid<float>::box(
    const nvector::View<float, 2>& view, float lat_min_p, float lat_max_p, float lon_min_p, float lon_max_p) const;
template nvector::View<int, 2> GeoGrid<double>::box(
    const nvector::View<int, 2>& view, double lat_min_p, double lat_max_p, double lon_min_p, double lon_max_p) const;
template nvector::View<int, 2> GeoGrid<float>::box(const nvector::View<int, 2>& view, float lat_min_p, float lat_max_p, float lon_min_p, float lon_max_p) const;

}  // namespace impactgen
