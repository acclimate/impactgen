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

#include "TimeVariable.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace impactgen {

TimeVariable::TimeVariable(const netCDF::NcFile& file, const std::string& filename, int time_shift) {
    const auto time_variable = file.getVar("time");
    const auto time_dimension = file.getDim("time");
    if (time_variable.isNull() || time_dimension.isNull()) {
        throw std::runtime_error(filename + ": No time information found");
    }
    if (!check_dimensions(time_variable, {"time"})) {
        throw std::runtime_error(filename + " - time: Unexpected dimensions");
    }
    std::string time_units;
    time_variable.getAtt("units").getValues(time_units);
    reference_time = ReferenceTime(time_units);
    times.resize(time_dimension.getSize());
    std::vector<int> tmp(times.size());
    time_variable.getVar({0}, {time_dimension.getSize()}, &tmp[0]);
    std::transform(std::begin(tmp), std::end(tmp), std::begin(times), [&](int t) -> std::time_t { return reference_time.unreference(t + time_shift); });
}

TimeVariable::TimeVariable(std::vector<std::time_t> times_p, ReferenceTime reference_time_p)
    : reference_time(std::move(reference_time_p)), times(std::move(times_p)) {}

void TimeVariable::write_to_file(const netCDF::NcFile& file, const ReferenceTime& reference_time) {
    const auto time_dimension = file.addDim("time", times.size());
    const auto time_variable = file.addVar("time", netCDF::NcType::nc_INT, time_dimension);
    time_variable.putAtt("calendar", "standard");  // TODO calendar
    time_variable.putAtt("units", reference_time.to_netcdf_format());
    std::vector<int> res(times.size());
    std::transform(std::begin(times), std::end(times), std::begin(res), [&](std::time_t t) -> int { return reference_time.reference(t); });
    time_variable.putVar({0}, {times.size()}, &res[0]);
}

}  // namespace impactgen
