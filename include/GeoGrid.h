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
#include "netcdf_headers.h"

namespace impactgen {

class GeoGrid {
  protected:
    unsigned int time_step = 0;
    float x_min = 0;
    float x_max = 0;
    float x_gridsize = 0;
    float t_x_gridsize = 0;
    float t_x_min = 0;
    float t_x_max = 0;
    std::size_t x_count = 0;
    float y_min = 0;
    float y_max = 0;
    float y_gridsize = 0;
    float t_y_gridsize = 0;
    float t_y_min = 0;
    float t_y_max = 0;
    std::size_t y_count = 0;

    unsigned int x_index(float x_var) const;
    unsigned int y_index(float y_var) const;

  public:
    class iterator {
      private:
        float l;
        std::size_t c;
        const float gridsize;
        const std::size_t count;

      public:
        using iterator_category = std::forward_iterator_tag;
        iterator(float l_, const std::size_t& c_, float gridsize_, const std::size_t& count_) : l(l_), c(c_), gridsize(gridsize_), count(count_) {}
        iterator operator++() {
            if (c < count) {
                c++;
                l += gridsize;
            }
            return *this;
        }
        float operator*() const { return l; }
        bool operator==(const iterator& rhs) const { return c == rhs.c; }
        bool operator!=(const iterator& rhs) const { return c != rhs.c; }
    };
    class X {
      protected:
        const GeoGrid& grid;

      public:
        explicit X(const GeoGrid& grid_) : grid(grid_) {}
        iterator begin() const { return iterator(grid.t_x_min, 0, grid.t_x_gridsize, grid.x_count); }
        iterator end() const { return iterator(grid.t_x_max, grid.x_count, grid.t_x_gridsize, grid.x_count); }
    };
    const X x;
    class Y {
      protected:
        const GeoGrid& grid;

      public:
        explicit Y(const GeoGrid& grid_) : grid(grid_) {}
        iterator begin() const { return iterator(grid.t_y_min, 0, grid.t_y_gridsize, grid.y_count); }
        iterator end() const { return iterator(grid.t_y_max, grid.y_count, grid.t_y_gridsize, grid.y_count); }
    };
    const Y y;
    inline float abs_x_gridsize() const { return t_x_gridsize; }
    inline float abs_y_gridsize() const { return t_y_gridsize; }

    GeoGrid() : x(*this), y(*this) {}
    explicit GeoGrid(const netCDF::NcFile& file);
    inline std::size_t size() const { return x_count * y_count; }
    inline std::size_t x_size() const { return x_count; }
    inline std::size_t y_size() const { return y_count; }
    inline float get_x(std::size_t x_) { return t_x_min + t_x_gridsize * x_; }
    inline float get_y(std::size_t y_) { return t_y_min + t_y_gridsize * y_; }
    float operator/(const GeoGrid& other) const;
    bool is_compatible(const GeoGrid& other) const;
    template<typename T>
    T read(float x_var, float y_var, const std::vector<T>& data) const;
    template<typename T>
    void write(float x_var, float y_var, std::vector<T>& data, T value) const;
};
}  // namespace impactgen

#endif
