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
#include <sstream>
#include <stdexcept>
#include <string>

namespace impactgen {

template<class Function>
inline std::string fill_template(const std::string& in, Function f) {
    const char* beg_mark = "[[";
    const char* end_mark = "]]";
    std::ostringstream ss;
    std::size_t pos = 0;
    while (true) {
        std::size_t start = in.find(beg_mark, pos);
        std::size_t stop = in.find(end_mark, start);
        if (stop == std::string::npos) {
            break;
        }
        ss.write(&*in.begin() + pos, start - pos);
        start += std::strlen(beg_mark);
        std::string key = in.substr(start, stop - start);
        ss << f(key);
        pos = stop + std::strlen(end_mark);
    }
    ss << in.substr(pos, std::string::npos);
    return ss.str();
}

inline std::string replace_all(const std::string& in, const std::string& to_replace, std::string replace_with) {
    auto res = in;
    auto pos = res.find(to_replace);
    while (pos != std::string::npos) {
        res.replace(pos, to_replace.size(), replace_with);
        pos = res.find(to_replace, pos + replace_with.size());
    }
    return res;
}

}  // namespace impactgen

#endif
