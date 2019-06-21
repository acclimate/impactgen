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

#include "impacts/AgentImpact.h"
#include <algorithm>
#include <string>
#include "settingsnode.h"

namespace impactgen {

void AgentImpact::read_sectors(const settings::SettingsNode& impact_node) {
    const auto& all_sectors = base_forcing.get_sectors();
    if (impact_node.has("sectors")) {
        auto seq = impact_node["sectors"].as_sequence();
        std::transform(std::begin(seq), std::end(seq), std::back_inserter(sectors),
                       [&](const settings::SettingsNode& s) -> int { return all_sectors.at(s.as<std::string>()); });
    } else {
        sectors.resize(all_sectors.size());
        std::transform(std::begin(all_sectors), std::end(all_sectors), std::begin(sectors),
                       [&](const std::pair<std::string, int>& s) -> int { return s.second; });
    }
}

}  // namespace impactgen
