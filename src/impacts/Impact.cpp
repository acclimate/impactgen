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

#include "impacts/Impact.h"
#include <algorithm>
#include <string>
#include "settingsnode.h"

namespace impactgen {

Impact::Impact(const settings::SettingsNode& impact_node) {
    time_shift = impact_node["time_shift"].as<int>(0);
    verbose = impact_node["verbose"].as<bool>(false);
    chunk_size = impact_node["chunk_size"].as<std::size_t>(1);
}

}  // namespace impactgen
