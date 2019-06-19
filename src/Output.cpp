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

#include "Output.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "TimeVariable.h"
#include "helpers.h"
#include "version.h"

#ifdef IMPACTGEN_HAS_DIFF
extern const char* impactgen_git_diff;
#endif

namespace impactgen {

Output::Output(const settings::SettingsNode& settings) {
    filename = fill_template(settings["output"]["file"].as<std::string>(), [&](const std::string& key) {
        if (settings.has(key)) {
            return settings[key].as<std::string>();
        } else {
            return std::string("UNKNOWN");
        }
    });
    reference_time = ReferenceTime(settings["reference"].as<std::string>());
    {
        std::ostringstream ss;
        ss << settings;
        settings_string = ss.str();
    }
    const auto& combo = settings["combination"].as<settings::hstring>();
    switch (combo) {
        case settings::hstring::hash("addition"):
        case settings::hstring::hash("add"):
            combination = ForcingCombination::ADD;
            break;
        case settings::hstring::hash("maximum"):
        case settings::hstring::hash("max"):
            combination = ForcingCombination::MAX;
            break;
        case settings::hstring::hash("minimum"):
        case settings::hstring::hash("min"):
            combination = ForcingCombination::MIN;
            break;
        case settings::hstring::hash("multiplication"):
        case settings::hstring::hash("mult"):
            combination = ForcingCombination::MULT;
            break;
        default:
            throw std::runtime_error("Unknown combination type '" + std::string(combo) + "'");
    }
}

void Output::append_array(const settings::SettingsNode& node, std::vector<std::string>& out) {
    const auto& type = node["type"].as<std::string>();
    if (type == "netcdf") {
        const auto& filename = node["file"].as<std::string>();
        netCDF::NcFile infile;
        try {
            infile.open(filename, netCDF::NcFile::read);
        } catch (netCDF::exceptions::NcException& e) {
            throw std::runtime_error(filename + ": " + e.what());
        }
        const auto variable = infile.getVar(node["variable"].as<std::string>());
        const auto count = variable.getDim(0).getSize();
        std::vector<char*> out_(count);
        variable.getVar({0}, {count}, &out_[0]);
        out.insert(std::end(out), std::begin(out_), std::end(out_));
    } else {
        throw std::runtime_error("Unknown input type " + type);
    }
}

void Output::add_regions(const settings::SettingsNode& regions_node) {
    if (!file.isNull()) {
        throw std::runtime_error("Cannot add regions after opening");
    }
    append_array(regions_node, regions);
}

void Output::add_sectors(const settings::SettingsNode& sectors_node) {
    if (!file.isNull()) {
        throw std::runtime_error("Cannot add regions after opening");
    }
    append_array(sectors_node, sectors);
}

void Output::open() {
    try {
        file.open(filename, netCDF::NcFile::replace, netCDF::NcFile::nc4);
    } catch (netCDF::exceptions::NcException& e) {
        throw std::runtime_error(filename + ": " + e.what());
    }

    {
        auto now = std::time(nullptr);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S");
        file.putAtt("created_at", ss.str());
    }
    file.putAtt("created_with", "impactgen");
    file.putAtt("impactgen_version", IMPACTGEN_VERSION);
#ifdef IMPACTGEN_HAS_DIFF
    file.putAtt("impactgen_diff", impactgen_git_diff);
#endif
    file.putAtt("settings", settings_string);

    agent_forcing = std::make_unique<ForcingSeries<AgentForcing>>(AgentForcing(sectors, regions), reference_time);
}

void Output::close() {
    TimeVariable time_variable(agent_forcing->get_sorted_times(), reference_time);
    time_variable.write_to_file(file, reference_time);
    const auto dim_time = file.getDim("time");

    const auto dim_sector = file.addDim("sector", sectors.size());
    {
        const auto var_sector = file.addVar("sector", netCDF::NcType::nc_STRING, {dim_sector});
        std::vector<const char*> sectors_chars(sectors.size());
        std::transform(std::begin(sectors), std::end(sectors), std::begin(sectors_chars), [](const std::string& s) -> const char* { return s.c_str(); });
        var_sector.putVar(&sectors_chars[0]);
    }

    const auto dim_region = file.addDim("region", regions.size());
    {
        const auto var_region = file.addVar("region", netCDF::NcType::nc_STRING, {dim_region});
        std::vector<const char*> regions_chars(regions.size());
        std::transform(std::begin(regions), std::end(regions), std::begin(regions_chars), [](const std::string& s) -> const char* { return s.c_str(); });
        var_region.putVar(&regions_chars[0]);
    }

    const auto var_agent_forcing = file.addVar("agent_forcing", netCDF::NcType::nc_FLOAT, {dim_time, dim_sector, dim_region});
    for (std::size_t t = 0; t < time_variable.times.size(); ++t) {
        var_agent_forcing.putVar({t, 0, 0}, {1, sectors.size(), regions.size()}, &agent_forcing->get_forcing(time_variable.times[t]).get_data()[0]);
    }
}

AgentForcing Output::prepare_forcing() const { return AgentForcing(agent_forcing->base_forcing); }

template<>
void Output::include_forcing<AgentForcing>(const ForcingSeries<AgentForcing>& forcing) {
    agent_forcing->include(forcing, combination);
}

}  // namespace impactgen
