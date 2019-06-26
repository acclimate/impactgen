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

#ifndef IMPACTGEN_IMPACT_H
#define IMPACTGEN_IMPACT_H

#include "helpers.h"
#include "settingsnode.h"

namespace impactgen {

class Output;

class Impact {
  protected:
    bool verbose;
    int time_shift;
    std::size_t chunk_size;

    explicit Impact(const settings::SettingsNode& impact_node);

  public:
    virtual void join(Output& output, const TemplateFunction& template_func) = 0;
    virtual ~Impact() {}
};

}  // namespace impactgen

#endif
