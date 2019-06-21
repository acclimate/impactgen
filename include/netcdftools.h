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

#ifndef IMPACTGEN_NETCDF_HEADERS_H
#define IMPACTGEN_NETCDF_HEADERS_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <ncDim.h>
#include <ncFile.h>
#include <ncGroupAtt.h>
#include <ncType.h>
#include <ncVar.h>
#include <netcdf>

#pragma GCC diagnostic pop

#include <algorithm>
#include <string>
#include <vector>

inline bool check_dimensions(const netCDF::NcVar& var, const std::vector<std::string>& names) {
    const auto& dims = var.getDims();
    if (dims.size() != names.size()) {
        return false;
    }
    for (int i = 0; i < names.size(); ++i) {
        if (dims[i].getName() != names[i]) {
            return false;
        }
    }
    return true;
}

#endif
