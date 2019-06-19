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

#ifndef IMPACTGEN_FLOODING_H
#define IMPACTGEN_FLOODING_H

#include <memory>
#include <vector>
#include "AgentForcing.h"
#include "Forcing.h"
#include "GeoGrid.h"
#include "Impact.h"
#include "nvector.h"
#include "settingsnode.h"

namespace impactgen {

class Output;

class Flooding : public Impact {
  protected:
    std::size_t chunk_size;
    bool verbose;
    nvector::Vector<int, 2> isoraster = nvector::Vector<int, 2>(-1, 0, 0);
    nvector::Vector<ForcingType, 2> last = nvector::Vector<ForcingType, 2>(0, 0, 0);
    std::unique_ptr<GeoGrid> isoraster_grid;
    ForcingType recovery_exponent;
    ForcingType recovery_threshold;
    std::string forcing_filename;
    std::string forcing_varname;
    std::string proxy_filename;
    std::string proxy_varname;
    std::vector<int> sectors;
    std::vector<int> regions;
    AgentForcing base_forcing;
    int time_shift;

  public:
    Flooding(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p);
    void join(Output& output, const TemplateFunction& template_func) override;
};
}  // namespace impactgen

#endif
