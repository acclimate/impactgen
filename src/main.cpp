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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include "Output.h"
#include "helpers.h"
#include "impacts/Flooding.h"
#include "impacts/TropicalCyclones.h"
#include "progressbar.h"
#include "settingsnode.h"
#include "settingsnode/inner.h"
#include "settingsnode/yaml.h"
#include "version.h"

#ifdef IMPACTGEN_HAS_DIFF
extern const char* impactgen_git_diff;
#endif
extern const char* impactgen_info;

static void run(const settings::SettingsNode& settings) {
    impactgen::Output output(settings);
    output.add_regions(settings["regions"]);
    output.add_sectors(settings["sectors"]);
    output.open();
    std::vector<settings::SettingsNode> impact_nodes;
    auto sequence = settings["impacts"].as_sequence();
    std::copy(std::begin(sequence), std::end(sequence), std::back_inserter(impact_nodes));
    progressbar::ProgressBar all_impacts_bar(impact_nodes.size(), "Impacts");
    for (const auto& impact_node : impact_nodes) {
        std::unique_ptr<impactgen::Impact> impact;
        std::string impact_name;
        const auto& type = impact_node["type"].as<settings::hstring>();
        switch (type) {
            // TODO event_series
            // TODO heat_labor_productivity
            // TODO shock
            case settings::hstring::hash("flooding"):
                impact = std::make_unique<impactgen::Flooding>(impact_node, output.prepare_forcing());
                impact_name = "Flooding";
                break;
            case settings::hstring::hash("tropical_cyclones"):
                impact = std::make_unique<impactgen::TropicalCyclones>(impact_node, output.prepare_forcing());
                impact_name = "Tropical Cyclones";
                break;
            default:
                throw std::runtime_error("Unsupported impact type '" + std::string(type) + "'");
        }
        std::size_t combination_count = 1;
        std::unordered_map<std::string, std::tuple<int, int, int>> range_variables;
        std::unordered_map<std::string, std::tuple<int, std::vector<std::string>>> sequence_variables;
        if (impact_node.has("variables")) {
            for (const auto& var : impact_node["variables"].as_map()) {
                if (var.second.is_sequence()) {
                    const auto seq = var.second.as_sequence();
                    std::vector<std::string> seq_values;
                    std::transform(std::begin(seq), std::end(seq), std::back_inserter(seq_values),
                                   [](const settings::SettingsNode& n) { return n.as<std::string>(); });
                    combination_count *= seq_values.size();
                    sequence_variables[var.first] = std::make_tuple(0, seq_values);
                } else {
                    const auto min_value = var.second["from"].as<int>();
                    const auto max_value = var.second["to"].as<int>();
                    if (min_value > max_value) {
                        throw std::runtime_error(var.first + ": 'from' value should be less than 'to' value");
                    }
                    combination_count *= (max_value - min_value + 1);
                    range_variables[var.first] = std::make_tuple(min_value, min_value, max_value);
                }
            }
        }
        progressbar::ProgressBar impact_bar(combination_count, impact_name, true);
        bool loop = true;
        while (loop) {
            impact->join(output, [&](const std::string& key, const std::string& temp) -> std::string {
                const auto& value = range_variables.find(key);
                if (value == std::end(range_variables)) {
                    const auto& seq = sequence_variables.find(key);
                    if (seq == std::end(sequence_variables)) {
                        throw std::runtime_error("Variable '" + key + "' not found for '" + temp + "'");
                    }
                    return std::get<1>(seq->second)[std::get<0>(seq->second)];
                }
                return std::to_string(std::get<0>(value->second));
            });
            loop = false;
            for (auto& var : range_variables) {
                auto& current_value = std::get<0>(var.second);
                const auto max_value = std::get<2>(var.second);
                if (current_value < max_value) {
                    ++current_value;
                    loop = true;
                    break;
                }
                const auto min_value = std::get<1>(var.second);
                current_value = min_value;
            }
            if (!loop) {
                for (auto& var : sequence_variables) {
                    auto& current_index = std::get<0>(var.second);
                    const auto& seq = std::get<1>(var.second);
                    if (current_index < seq.size() - 1) {
                        ++current_index;
                        loop = true;
                        break;
                    }
                    current_index = 0;
                }
            }
            ++impact_bar;
        }
        impact_bar.close(true);
        ++all_impacts_bar;
    }
    output.close();
    all_impacts_bar.close();
}

static void print_usage(const char* program_name) {
    std::cerr << "ImpactGen - impact generator / preprocessing for the Acclimate model\n"
                 "Version: " IMPACTGEN_VERSION
                 "\n\n"
                 "Authors: Sven Willner <sven.willner@pik-potsdam.de>\n"
                 "         Kilian Kuhla <kilian.kuhla@pik-potsdam.de>\n"
                 "\n"
                 "Usage:   "
              << program_name
              << " (<option> | <settingsfile>)\n"
                 "Options:\n"
#ifdef IMPACTGEN_HAS_DIFF
                 "  -d, --diff     Print git diff output from compilation\n"
#endif
                 "  -h, --help     Print this help text\n"
                 "  -v, --version  Print version"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    const std::string arg = argv[1];
    if (arg.length() > 1 && arg[0] == '-') {
        if (arg == "--version" || arg == "-v") {
            std::cout << IMPACTGEN_VERSION << std::endl;
#ifdef IMPACTGEN_HAS_DIFF
        } else if (arg == "--diff" || arg == "-d") {
            std::cout << impactgen_git_diff << std::flush;
#endif
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    } else {
#ifndef DEBUG
        try {
#endif
            if (arg == "-") {
                std::cin >> std::noskipws;
                run(settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(std::cin))));
            } else {
                std::ifstream settings_file(arg);
                if (!settings_file) {
                    throw std::runtime_error("Cannot open " + arg);
                }
                run(settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(settings_file))));
            }
#ifndef DEBUG
        } catch (std::runtime_error& ex) {
            std::cerr << ex.what() << std::endl;
            return 255;
        }
#endif
    }
    return 0;
}
