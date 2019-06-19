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

#ifndef IMPACTGEN_HELPERS_H
#define IMPACTGEN_HELPERS_H

#include <cstring>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace impactgen {

using TemplateFunction = const std::function<std::string(const std::string&, const std::string&)>;

extern std::string fill_template(const std::string& in, const TemplateFunction& f);
extern std::string replace_all(const std::string& in, const std::string& to_replace, std::string replace_with);

}  // namespace impactgen

#endif
