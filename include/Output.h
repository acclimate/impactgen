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

#ifndef IMPACTGEN_OUTPUT_H
#define IMPACTGEN_OUTPUT_H

#include <ctime>
#include <memory>
#include <string>
#include <vector>
#include "AgentForcing.h"
#include "Forcing.h"
#include "ForcingSeries.h"
#include "ReferenceTime.h"
#include "netcdftools.h"
#include "settingsnode.h"

namespace impactgen {

class AgentForcing;

class Output {
  protected:
    std::unique_ptr<ForcingSeries<AgentForcing>> agent_forcing;
    std::vector<std::string> regions;
    std::vector<std::string> sectors;
    ReferenceTime reference_time;
    std::string filename;
    std::string settings_string;
    netCDF::NcFile file;
    void append_array(const settings::SettingsNode& node, std::vector<std::string>& out);
    ForcingCombination combination;

  public:
    explicit Output(const settings::SettingsNode& settings);
    const ReferenceTime& ref() const { return reference_time; }
    const std::vector<std::string>& get_regions() const { return regions; }
    void add_regions(const settings::SettingsNode& regions_node);
    void add_sectors(const settings::SettingsNode& sectors_node);
    template<class Forcing>
    void include_forcing(const ForcingSeries<Forcing>& forcing);
    AgentForcing prepare_forcing() const;
    void open();
    void close();
};
}  // namespace impactgen

#endif
