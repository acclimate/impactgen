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
#include <limits>
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

    GeoGrid() = default;
    void read_from_netcdf(const netCDF::NcFile& file, const std::string& filename);
    constexpr std::size_t size() const { return lat_count * lon_count; }
    constexpr inline T lat(std::size_t lat_index) { return lat_min + lat_abs_stepsize * lat_index; }
    constexpr inline T lon(std::size_t lon_index) { return lon_min + lon_abs_stepsize * lon_index; }
    constexpr std::size_t lat_index(T lat) const {
        T res;
        if (lat_stepsize < 0) {
            res = (lat_max - lat) * lat_count / (lat_max - lat_min - lat_stepsize);
        } else {
            res = (lat - lat_min) * lat_count / (lat_max - lat_min + lat_stepsize);
        }
        if (res < 0 || res >= lat_count) {
            return std::numeric_limits<std::size_t>::quiet_NaN();
        }
        return static_cast<std::size_t>(res);
    }
    constexpr std::size_t lon_index(T lon) const {
        T res;
        if (lon_stepsize < 0) {
            res = (lon_max - lon) * lon_count / (lon_max - lon_min - lon_stepsize);
        } else {
            res = (lon - lon_min) * lon_count / (lon_max - lon_min + lon_stepsize);
        }
        if (res < 0 || res >= lon_count) {
            return std::numeric_limits<std::size_t>::quiet_NaN();
        }
        return static_cast<std::size_t>(res);
    }
    constexpr T operator/(const GeoGrid<T>& other) const { return lat_abs_stepsize * lon_abs_stepsize / other.lat_abs_stepsize / other.lon_abs_stepsize; }
    constexpr bool is_compatible(const GeoGrid<T>& other) const {
        return std::abs(lat_abs_stepsize - other.lat_abs_stepsize) / lat_abs_stepsize < 1e-2
               && std::abs(lon_abs_stepsize - other.lon_abs_stepsize) / lon_abs_stepsize < 1e-2;
    }
    template<typename V>
    constexpr nvector::View<V, 2> box(
        const nvector::View<V, 2>& view, T lat_min_p, T lat_max_p, T lon_min_p, T lon_max_p, std::size_t max_lat_size, std::size_t max_lon_size) const {
        const auto& lat_slice = view.template slice<0>();
        const auto& lon_slice = view.template slice<1>();
        nvector::Slice new_lat_slice;
        nvector::Slice new_lon_slice;

        const auto lat_min_index = lat_index(lat_min_p);
        const auto lat_max_index = lat_index(lat_max_p);
        if (lat_min_index > lat_max_index) {
            new_lat_slice.begin = -lat_slice.begin - lat_min_index;
            new_lat_slice.size = std::min(lat_min_index - lat_max_index, max_lat_size);
            new_lat_slice.stride = -lat_slice.stride;
        } else {
            new_lat_slice.begin = lat_slice.begin + lat_min_index;
            new_lat_slice.size = std::min(lat_max_index - lat_min_index, max_lat_size);
            new_lat_slice.stride = lat_slice.stride;
        }

        const auto lon_min_index = lon_index(lon_min_p);
        const auto lon_max_index = lon_index(lon_max_p);
        if (lon_min_index > lon_max_index) {
            new_lon_slice.begin = -lon_slice.begin - lon_min_index;
            new_lon_slice.size = std::min(lon_min_index - lon_max_index, max_lon_size);
            new_lon_slice.stride = -lon_slice.stride;
        } else {
            new_lon_slice.begin = lon_slice.begin + lon_min_index;
            new_lon_slice.size = std::min(lon_max_index - lon_min_index, max_lon_size);
            new_lon_slice.stride = lon_slice.stride;
        }

        return typename nvector::View<V, 2>(view.data(), {new_lat_slice, new_lon_slice});
    }
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

template<typename V, typename T = float>
struct GridView {
    nvector::View<V, 2>& data;
    GeoGrid<T>& grid;
};

template<typename T, typename... Args>
inline bool all_compatible(const GeoGrid<T>& grid1, const GeoGrid<T>& grid2) {
    return grid1.is_compatible(grid2);
}

template<typename T, typename... Args>
inline bool all_compatible(const GeoGrid<T>& grid1, const GeoGrid<T>& grid2, const Args&... grids) {
    return grid1.is_compatible(grid2) && all_compatible(grid2, grids...);
}

template<typename T, typename... Args>
inline auto common_grid_view(GeoGrid<T>& common_grid, const Args&... grid_views) -> decltype(std::make_tuple(grid_views.data...)) {
    common_grid.lat_min = reduce(max<T>, grid_views.grid.lat_min...);
    common_grid.lat_max = reduce(min<T>, grid_views.grid.lat_max...);
    common_grid.lon_min = reduce(max<T>, grid_views.grid.lon_min...);
    common_grid.lon_max = reduce(min<T>, grid_views.grid.lon_max...);
    common_grid.lat_count = reduce(min<long>, std::abs(static_cast<long>(grid_views.grid.lat_index(common_grid.lat_min))
                                                       - static_cast<long>(grid_views.grid.lat_index(common_grid.lat_max)))...);
    common_grid.lon_count = reduce(min<long>, std::abs(static_cast<long>(grid_views.grid.lon_index(common_grid.lon_min))
                                                       - static_cast<long>(grid_views.grid.lon_index(common_grid.lon_max)))...);
    common_grid.lat_stepsize = common_grid.lat_abs_stepsize = std::get<0>(std::make_tuple(grid_views.grid...)).lat_abs_stepsize;
    common_grid.lon_stepsize = common_grid.lon_abs_stepsize = std::get<0>(std::make_tuple(grid_views.grid...)).lon_abs_stepsize;
    if (!all_compatible(grid_views.grid...)) {
        throw std::runtime_error("Grid sizes do not match");
    }
    return std::make_tuple(grid_views.grid.box(grid_views.data, common_grid.lat_min, common_grid.lat_max, common_grid.lon_min, common_grid.lon_max,
                                               common_grid.lat_count, common_grid.lon_count)...);
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
