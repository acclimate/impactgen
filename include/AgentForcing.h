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

#ifndef IMPACTGEN_AGENTFORCING_H
#define IMPACTGEN_AGENTFORCING_H

#include <ctime>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Forcing.h"

namespace impactgen {

class AgentForcing {
  protected:
    std::shared_ptr<std::unordered_map<std::string, std::size_t>> sectors;
    std::shared_ptr<std::unordered_map<std::string, std::size_t>> regions;
    std::vector<ForcingType> data;

  public:
    AgentForcing() {}
    AgentForcing(const std::vector<std::string>& sectors_p, const std::vector<std::string>& regions_p);
    const std::unordered_map<std::string, std::size_t>& get_sectors() const;
    const std::unordered_map<std::string, std::size_t>& get_regions() const;
    ForcingType operator()(const std::string& sector, const std::string& region) const;
    ForcingType operator()(std::size_t sector, std::size_t region) const;
    ForcingType& operator()(const std::string& sector, const std::string& region);
    ForcingType& operator()(std::size_t sector, std::size_t region);
    void include(const AgentForcing& other, ForcingCombination combination);
    const std::vector<ForcingType>& get_data() const { return data; }
};

}  // namespace impactgen

#endif
