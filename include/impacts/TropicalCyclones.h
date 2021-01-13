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

#ifndef IMPACTGEN_TROPICAL_CYCLONES_H
#define IMPACTGEN_TROPICAL_CYCLONES_H

#include <random>
#include <string>
#include "impacts/AgentImpact.h"
#include "impacts/Impact.h"
#include "impacts/ProxiedImpact.h"
#include "settingsnode.h"

namespace impactgen {

class Output;

class TropicalCyclones : public AgentImpact, public ProxiedImpact, public Impact {
  protected:
    std::string forcing_filename;
    std::string forcing_varname;
    std::string events_varname;
    std::string basin;
    int year_from;
    int year_to;
    std::size_t realization;
    float threshold;
    float velocity;
    std::mt19937 random_generator;
    std::unordered_map<std::string, std::pair<int, int>> seasons;
    ForcingType recovery_exponent;
    ForcingType recovery_threshold;
    ForcingType windspeed_threshold;

  public:
    TropicalCyclones(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p);
    void join(Output& output, const TemplateFunction& template_func) override;
};
}  // namespace impactgen

#endif
