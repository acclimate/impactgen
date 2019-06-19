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

#include "AgentForcing.h"
#include <memory>
#include <stdexcept>

namespace impactgen {

AgentForcing::AgentForcing(const std::vector<std::string>& sectors_p, const std::vector<std::string>& regions_p)
    : sectors(std::make_shared<std::unordered_map<std::string, std::size_t>>(sectors_p.size())),
      regions(std::make_shared<std::unordered_map<std::string, std::size_t>>(regions_p.size())) {
    for (std::size_t i = 0; i < sectors_p.size(); ++i) {
        sectors->emplace(sectors_p[i], i);
    }
    for (std::size_t i = 0; i < regions_p.size(); ++i) {
        regions->emplace(regions_p[i], i);
    }
    data = std::vector<ForcingType>(sectors->size() * regions->size(), 0);
}

const std::unordered_map<std::string, std::size_t>& AgentForcing::get_sectors() const { return *sectors; }

const std::unordered_map<std::string, std::size_t>& AgentForcing::get_regions() const { return *regions; }

ForcingType AgentForcing::operator()(const std::string& sector, const std::string& region) const {
    return data[sectors->at(sector) * regions->size() + regions->at(region)];
}

ForcingType AgentForcing::operator()(std::size_t sector, std::size_t region) const { return data[sector * regions->size() + region]; }

ForcingType& AgentForcing::operator()(const std::string& sector, const std::string& region) {
    return data[sectors->at(sector) * regions->size() + regions->at(region)];
}

ForcingType& AgentForcing::operator()(std::size_t sector, std::size_t region) { return data[sector * regions->size() + region]; }

void AgentForcing::include(const AgentForcing& other, ForcingCombination combination) {
    if (sectors.get() != other.sectors.get() || regions.get() != other.regions.get()) {
        throw std::runtime_error("Forcings are not related");
    }
    switch (combination) {
        case ForcingCombination::ADD:
            for (std::size_t i = 0; i < data.size(); ++i) {
                data[i] = std::max(data[i] + other.data[i] - 1, ForcingType(0.0));
            }
            break;
        case ForcingCombination::MAX:
            for (std::size_t i = 0; i < data.size(); ++i) {
                data[i] = std::max(data[i], other.data[i]);
            }
            break;
        case ForcingCombination::MIN:
            for (std::size_t i = 0; i < data.size(); ++i) {
                data[i] = std::min(data[i], other.data[i]);
            }
            break;
        case ForcingCombination::MULT:
            for (std::size_t i = 0; i < data.size(); ++i) {
                data[i] = data[i] * other.data[i];
            }
            break;
    }
}

}  // namespace impactgen
