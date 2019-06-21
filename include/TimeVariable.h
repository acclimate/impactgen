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

#ifndef IMPACTGEN_TIMEVARIABLE_H
#define IMPACTGEN_TIMEVARIABLE_H

#include <ctime>
#include <vector>
#include "ReferenceTime.h"
#include "netcdftools.h"

namespace impactgen {

class TimeVariable {
  protected:
    ReferenceTime reference_time;

  public:
    std::vector<std::time_t> times;

    explicit TimeVariable(const netCDF::NcFile& file, const std::string& filename, int time_shift = 0);
    explicit TimeVariable(std::vector<std::time_t> times_p, ReferenceTime reference_time_p);
    inline const ReferenceTime& ref() const { return reference_time; }
    void write_to_file(const netCDF::NcFile& file, const ReferenceTime& reference_time_p);
};

}  // namespace impactgen

#endif
