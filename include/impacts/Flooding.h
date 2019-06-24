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

#include <string>
#include "impacts/AgentImpact.h"
#include "impacts/Impact.h"
#include "impacts/ProxiedImpact.h"
#include "settingsnode.h"
#include "GeoGrid.h"

namespace impactgen {

class Output;

class Flooding : public AgentImpact, public ProxiedImpact, public Impact {
  protected:
    nvector::Vector<ForcingType, 2> last;
    GeoGrid<float> last_grid;
    ForcingType recovery_exponent;
    ForcingType recovery_threshold;
    std::string forcing_filename;
    std::string forcing_varname;

  public:
    Flooding(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p);
    void join(Output& output, const TemplateFunction& template_func) override;
};
}  // namespace impactgen

#endif
