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

#include "ReferenceTime.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "helpers.h"

namespace impactgen {

static std::pair<std::time_t, std::size_t> parse_netcdf_format(const std::string& netcdf_format) {
    std::tm res = {};
    std::istringstream ss(netcdf_format);
    ss >> std::get_time(&res, "days since %Y-%m-%d");
    if (!ss.fail()) {
        return std::make_pair(std::mktime(&res), 24 * 60 * 60);
    }
    ss.clear();
    ss >> std::get_time(&res, "hours since %Y-%m-%d %H:00");
    if (!ss.fail()) {
        return std::make_pair(std::mktime(&res), 60 * 60);
    }
    ss.clear();
    ss >> std::get_time(&res, "minutes since %Y-%m-%d %H:%M");
    if (!ss.fail()) {
        return std::make_pair(std::mktime(&res), 60);
    }
    ss.clear();
    ss >> std::get_time(&res, "minutes since %Y-%m-%d %H:%M:00");
    if (!ss.fail()) {
        return std::make_pair(std::mktime(&res), 60);
    }
    ss.clear();
    ss >> std::get_time(&res, "seconds since %Y-%m-%d %H:%M:%S");
    if (!ss.fail()) {
        return std::make_pair(std::mktime(&res), 1);
    }
    return std::make_pair(-1, 0);
}

ReferenceTime::ReferenceTime(const std::string& netcdf_format) {
    auto p = parse_netcdf_format(netcdf_format);
    if (p.second == 0) {
        p = parse_netcdf_format(replace_all(netcdf_format, "-", "-0"));
    }
    if (p.second == 0) {
        throw std::runtime_error("Unknown time reference '" + netcdf_format + "'");
    }
    std::tie(time, accuracy) = p;
}

int ReferenceTime::reference(std::time_t time_p) const { return (time_p - time) / accuracy; }
std::time_t ReferenceTime::unreference(int time_p) const { return time_p * accuracy + time; }
bool ReferenceTime::compatible_with(const ReferenceTime& other) const { return other.accuracy == accuracy; }

std::string ReferenceTime::to_netcdf_format() const {
    std::ostringstream res;
    switch (accuracy) {
        case 1:
            res << "seconds since " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
            break;
        case 60:
            res << "minutes since " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M");
            break;
        case 60 * 60:
            res << "hours since " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:00");
            break;
        case 24 * 60 * 60:
            res << "days since " << std::put_time(std::localtime(&time), "%Y-%m-%d");
            break;
        default:
            throw std::runtime_error("Invalid accuracy of " + std::to_string(accuracy));
    }
    return res.str();
}

}  // namespace impactgen
