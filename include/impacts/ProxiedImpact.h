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

#ifndef IMPACTGEN_PROXIED_IMPACT_H
#define IMPACTGEN_PROXIED_IMPACT_H

#include <string>
#include <vector>
#include "Forcing.h"
#include "GeoGrid.h"
#include "impacts/GriddedImpact.h"
#include "nvector.h"
#include "settingsnode.h"

namespace impactgen {

class ProxiedImpact : public GriddedImpact {
  protected:
    bool verbose;
    std::string proxy_filename;
    std::string proxy_varname;
    GeoGrid<float> proxy_grid;
    std::vector<ForcingType> total_proxy;
    nvector::Vector<ForcingType, 2> proxy_values;

    ProxiedImpact(const settings::SettingsNode& proxy_node);
    void read_proxy(const std::string& filename, const std::vector<std::string>& all_regions);
};

}  // namespace impactgen

#endif
