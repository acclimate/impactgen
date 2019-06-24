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

#ifndef IMPACTGEN_GEOGRID_H
#define IMPACTGEN_GEOGRID_H

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "netcdftools.h"
#include "nvector.h"

namespace impactgen {

template<typename T>
struct GeoGrid {
    T lon_min = 0;
    T lon_max = 0;
    T lon_stepsize = 0;
    T lon_abs_stepsize = 0;
    std::size_t lon_count = 0;
    T lat_min = 0;
    T lat_max = 0;
    T lat_stepsize = 0;
    T lat_abs_stepsize = 0;
    std::size_t lat_count = 0;

    GeoGrid() {}
    void read_from_netcdf(const netCDF::NcFile& file, const std::string& filename);
    inline std::size_t size() const { return lat_count * lon_count; }
    std::size_t lat_index(T lat) const;
    std::size_t lon_index(T lon) const;
    inline T lat(std::size_t lat_index) { return lat_min + lat_abs_stepsize * lat_index; }
    inline T lon(std::size_t lon_index) { return lon_min + lon_abs_stepsize * lon_index; }
    T operator/(const GeoGrid& other) const;
    bool is_compatible(const GeoGrid& other) const;
    template<typename V>
    nvector::View<V, 2> box(
        const nvector::View<V, 2>& view, T lat_min_p, T lat_max_p, T lon_min_p, T lon_max_p, std::size_t max_lat_size, std::size_t max_lon_size) const;
};

template<typename Func, typename T>
T reduce(const Func& f, T v1, T v2) {
    return f(v1, v2);
}

template<typename Func, typename T, typename... Args>
T reduce(const Func& f, T v1, T v2, Args... others) {
    return reduce(f, f(v1, v2), others...);
}

template<typename T>
inline T max(T a, T b) {
    return std::max(a, b);
}
template<typename T>
inline T min(T a, T b) {
    return std::min(a, b);
}

template<typename T>
struct GridView {
    nvector::View<T, 2>& data;
    GeoGrid<float>& grid;
};

template<typename... Args>
inline auto common_grid_box(const Args&... grid_views) -> decltype(std::make_tuple(grid_views.data...)) {
    const auto lat_min = reduce(max<float>, grid_views.grid.lat_min...);
    const auto lat_max = reduce(min<float>, grid_views.grid.lat_max...);
    const auto lon_min = reduce(max<float>, grid_views.grid.lon_min...);
    const auto lon_max = reduce(min<float>, grid_views.grid.lon_max...);
    const auto max_lat_size =
        reduce(min<int>, std::abs(static_cast<int>(grid_views.grid.lat_index(lat_min)) - static_cast<int>(grid_views.grid.lat_index(lat_max)))...);
    const auto max_lon_size =
        reduce(min<int>, std::abs(static_cast<int>(grid_views.grid.lon_index(lon_min)) - static_cast<int>(grid_views.grid.lon_index(lon_max)))...);
    return std::make_tuple(grid_views.grid.box(grid_views.data, lat_min, lat_max, lon_min, lon_max, max_lat_size, max_lon_size)...);
}

template<typename V>
inline void print_view(const nvector::View<V, 2>& view, const std::size_t width = 50) {
    const auto& lat_slice = view.template slice<0>();
    const auto& lon_slice = view.template slice<1>();
    const std::size_t agg_size = lon_slice.size / width;
    for (std::size_t lat_chunk = 0; lat_chunk < lat_slice.size; lat_chunk += agg_size) {
        for (std::size_t lon_chunk = 0; lon_chunk < lon_slice.size; lon_chunk += agg_size) {
            std::size_t count = 0;
            for (std::size_t lat = lat_chunk; lat < std::min(lat_slice.size, lat_chunk + agg_size); ++lat) {
                for (std::size_t lon = lon_chunk; lon < std::min(lon_slice.size, lon_chunk + agg_size); ++lon) {
                    if (view(lat_slice.size - 1 - lat, lon) > 0) {
                        ++count;
                    }
                }
            }
            if (5 * count > agg_size * agg_size) {
                std::cout << 'x';
            } else {
                std::cout << ' ';
            }
        }
        std::cout << '\n';
    }
    std::cout << std::flush;
}

}  // namespace impactgen

#endif
