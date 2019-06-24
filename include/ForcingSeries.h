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

#ifndef IMPACTGEN_FORCINGSERIES_H
#define IMPACTGEN_FORCINGSERIES_H

#include <algorithm>
#include <ctime>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "Forcing.h"
#include "ReferenceTime.h"

namespace impactgen {

template<class Forcing>
class ForcingSeries {
  protected:
    std::unordered_map<int, Forcing> data;

  public:
    const ReferenceTime reference_time;
    const Forcing base_forcing;
    ForcingSeries() {}
    ForcingSeries(Forcing base_forcing_p, ReferenceTime reference_time_p)
        : base_forcing(std::move(base_forcing_p)), reference_time(std::move(reference_time_p)) {}

    Forcing& insert_forcing(std::time_t time) {
        const auto t = reference_time.reference(time);
        if (data.find(t) != std::end(data)) {
            throw std::runtime_error("Time already set");
        }
        return data.emplace(t, Forcing(base_forcing)).first->second;
    }

    Forcing& get_forcing(std::time_t time) { return data.at(reference_time.reference(time)); }

    std::vector<std::time_t> get_sorted_times() const {
        std::vector<std::time_t> res(data.size());
        int i = 0;
        for (const auto& d : data) {
            res[i] = reference_time.unreference(d.first);
            ++i;
        }
        std::sort(std::begin(res), std::end(res));
        return res;
    }

    void include(const ForcingSeries<Forcing>& other, ForcingCombination combination) {
        if (!reference_time.compatible_with(other.reference_time)) {
            throw std::runtime_error("Incompatible accuracies");
        }
        for (const auto& other_forcing : other.data) {
            const auto t = reference_time.reference(other.reference_time.unreference(other_forcing.first));
            auto forcing = data.find(t);
            if (forcing == std::end(data)) {
                data[t] = other_forcing.second;
            } else {
                forcing->second.include(other_forcing.second, combination);
            }
        }
    }
};

}  // namespace impactgen

#endif
