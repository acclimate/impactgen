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

#ifndef IMPACTGEN_REFERENCE_TIME_H
#define IMPACTGEN_REFERENCE_TIME_H

#include <ctime>
#include <string>

namespace impactgen {

class ReferenceTime {
  protected:
    std::time_t time;
    std::size_t accuracy;

  public:
    explicit ReferenceTime(std::time_t time_p = -1, std::size_t accuracy_p = 1) : time(time_p), accuracy(accuracy_p) {}
    explicit ReferenceTime(const std::string& netcdf_format);
    static std::time_t year(int year_p);
    std::string to_netcdf_format() const;
    int reference(std::time_t time_p) const;
    std::time_t unreference(int time_p) const;
    bool compatible_with(const ReferenceTime& other) const;
};

}  // namespace impactgen

#endif
