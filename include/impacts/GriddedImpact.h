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

#ifndef IMPACTGEN_GRIDDED_IMPACT_H
#define IMPACTGEN_GRIDDED_IMPACT_H

#include <unordered_map>
#include <vector>
#include "GeoGrid.h"
#include "nvector.h"
#include "settingsnode.h"

namespace impactgen {

class GriddedImpact {
  protected:
    nvector::Vector<int, 2> isoraster;
    GeoGrid<float> isoraster_grid;
    std::vector<int> regions;

    void read_isoraster(const settings::SettingsNode& isoraster_node, const std::unordered_map<std::string, std::size_t>& all_regions);
};

}  // namespace impactgen

#endif
