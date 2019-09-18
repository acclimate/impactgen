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
#include <vector>
#include <cstdlib>
#include <cmath>
#include "Output.h"
#include "helpers.h"
#include "impacts/Flooding.h"
#include "impacts/HeatLaborProductivity.h"
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

YAML::Node config = YAML::LoadFile("config.yaml");  // starts out as null

std::string trading_economics_dir = config["TE_dir"].as<std::string>();
std::vector<std::string> regions = config["regions"].as<std::vector<std::string>>();
std::vector<std::string> sectors = config["sectors"].as<std::vector<std::string>>();
std::vector<int> years_to_observe = config["years_to_observe"].as<std::vector<int>>();
int year_validation = config["year_validation"].as<int>();
int num_params_per_region = config["num_params_per_region"].as<int>();
float params_min = config["params_min"].as<float>();
float params_max = config["params_max"].as<float>();

// std::vector<std::string> sectors = {"Manufacturing Production", "Industrial Production", "Capacity Utilization", "TREQ",
//                                     "Steel Production",         "Electricity Production"};
//
// std::string trading_economics_dir = "/p/projects/zeean/calibration/tradingeconomics_data/";
// // int event_hurricane_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0};   // for Hurricane: Aug, Sep, Oct
// // int event_heatstress_months_to_observe[12] = {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0};  // for Heatstress: Jun, Jul, Aug
// // int event_flooding_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0};    // for Flooding: Aug, Sep, Oct
// std::vector<int> years_to_observe = {2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009};
// int year_validation = 2010;

// NLD: earliest data gathering for NLD is Jan-2001
// this means missing data handling mechanism needs to be in place
// CHE: has only quarterly data starting from Jan-2000.
// SAU: only steel production has monthly data starting from Jan-2000, all other
// are quarterly or start from 2016
// std::vector<std::string> regions = {"USA", "CHN", "JPN", "DEU", "GBR", "FRA", "IND", "ITA", "BRA", "CAN", "KOR", "RUS", "ESP", "AUS", "MEX", "IDN", "TUR",
//                                     "SAU",
//                                     // "NLD", "CHE",
//                                     "ARG", "ZAF", "SGP", "THA"};
//
// int num_params_per_region = 10;
//
// // TODO decide on float vs double throughout. Use using FloatType = ...?
//
// float params_min = 0.0;
// float params_max = 1.0;

struct TimeRange {
    int begin;
    int count;
};

template<typename T>
static T random_between(T a, T b) {
    T random = static_cast<T>(rand()) / static_cast<T>(RAND_MAX);
    T diff = b - a;
    T r = random * diff;
    return a + r;
}

/** is_leap_year
    Function will return true or false of leap year, depending on year

    @param year Type int
    @return boolean result
*/
static inline bool is_leap_year(int year) { return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0); }

/** get_number_of_days
    Function will return total number of days for month, year combination

    @param month Type int
    @param year Type int
    @return int result
*/
static int get_number_of_days(int month, int year) {
    // leap year condition, if month is 2
    if (month == 2) {
        if (is_leap_year(year)) {
            return 29;
        }
        return 28;
    }
    // months which has 31 days
    if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) {
        return 31;
    }
    return 30;
}

/** initialize_te_data
    Function will initialize trading_economics_data by reference

    @param trading_economics_data Type std::unordered_map<std::string,
   std::vector<float>>
    @return void
*/
static void initialize_te_data(std::unordered_map<std::string, std::vector<float>>& trading_economics_data) {
    // sector preference
    for (std::size_t i = 0; i < regions.size(); ++i) {  // TODO use for-each loop
        std::vector<float> tmp_val_vector;
        for (std::size_t j = 0; j < sectors.size(); ++j) {  // TODO use for-each loop
            std::ifstream ifile;
            std::string tmp_te_file = trading_economics_dir + "production_" + regions[i] + "_" + sectors[j] + ".csv";

            ifile.open(tmp_te_file);

            int tmp_year = 2000;
            int tmp_month = 1;
            if (!ifile.fail()) {  // if the file can be found
                std::string line;
                int tmp_idx = 0;
                while (std::getline(ifile, line))
                {
                    if (tmp_idx > 0) {
                        std::istringstream iss{line};

                        std::vector<std::string> tokens;
                        std::string token;
                        while (std::getline(iss, token, '\t')) {
                            tokens.push_back(token);
                        }
                        // map the tokens into our variables
                        std::string month = tokens[0];
                        float value = std::stod(tokens[1]);
                        std::string frequency = tokens[3];

                        if (frequency != "Monthly" || std::stoi(month.substr(0, 4)) != tmp_year || std::stoi(month.substr(5, 2)) != tmp_month) {
                            break;
                        }
                        // if (event_hurricane_months_to_observe[tmp_month - 1] || event_heatstress_months_to_observe[tmp_month - 1]
                        //     || event_flooding_months_to_observe[tmp_month - 1]) {
                        //     tmp_val_vector.push_back(value);
                        // }
                        tmp_val_vector.push_back(value);

                        if (tmp_month == 12) {
                            tmp_month = 1;
                            tmp_year++;
                        } else {
                            tmp_month++;
                        }
                    }
                    tmp_idx++;
                }
            }
            if (tmp_val_vector.size() > 0) {
                trading_economics_data.emplace(regions[i], tmp_val_vector);
                break;
            }
        }
        // if (trading_economics_data[regions[i]].size()>0)
        // {
        //   std::cout << regions[i] << "\t";
        //   for (auto j = trading_economics_data[regions[i]].begin(); j !=
        //   trading_economics_data[regions[i]].end(); ++j)
        //     std::cout << *j << ' ';
        //   std::cout << std::endl;
        // }
        // else
        // {
        //   std::cout << regions[i] << "\t No suitable data found" << std::endl;
        // }
    }
}

/** initialize_times
    Function will initialize times by reference

    @param times Type std::vector<TimeRange>
    @return void
*/
static void initialize_times(std::vector<TimeRange>& times) {
    int tmp_idx = 0;
    for (std::size_t i = 0; i < years_to_observe.size(); ++i) {
        for (int j = 0; j < 12; ++j) {
            int tmp_num_day = get_number_of_days(j, years_to_observe[i]);
            // if (event_hurricane_months_to_observe[j] || event_heatstress_months_to_observe[j] || event_flooding_months_to_observe[j]) {
            //     times.push_back({tmp_idx + tmp_num_day, tmp_num_day});
            // }
            times.push_back({tmp_idx + tmp_num_day, tmp_num_day});
            tmp_idx += tmp_num_day;
        }
    }
}

/** initialize_parameters
    Function will initialize parameters by reference

    @param times Type std::vector<TimeRange>
    @return void
*/
static void initialize_parameters(std::unordered_map<std::string, std::vector<float>>& parameters) {
    for (std::size_t i = 0; i < regions.size(); ++i) {
        std::vector<float> tmp_params_vector;
        for (int j = 0; j < num_params_per_region; j++) {
            tmp_params_vector.push_back(random_between(params_min, params_max));
        }
        parameters.emplace(regions[i], tmp_params_vector);
    }
}

/** loss_value()
    Function will calculate the loss_value by comparing trading_economics_data and model_forecast_data
    currently loss_value is MPE (Mean Percentage Error)
    @param trading_economics_data Type std::unordered_map<std::string, std::vector<float>>
    @param model_forecast_data Type std::unordered_map<std::string, std::vector<float>>
    @return float
*/
static float loss_value(std::unordered_map<std::string, std::vector<float>> trading_economics_data,
                       std::unordered_map<std::string, std::vector<float>> model_forecast_data)
{
    float loss_sum = 0;
    for (auto it : trading_economics_data) {
        std::vector<float> tmp_te_vector = trading_economics_data[it.first];
        std::vector<float> tmp_forecast_vector = model_forecast_data[it.first];
        for (std::size_t j = 0; j < years_to_observe.size(); ++j)
        {
            loss_sum += abs(tmp_te_vector[j]-tmp_forecast_vector[j]) / sqrt(tmp_te_vector[j] * tmp_te_vector[j] + tmp_forecast_vector[j] * tmp_forecast_vector[j]);
        }
    }
    return loss_sum/float(trading_economics_data.size()*years_to_observe.size());
}

static float generate_impact(std::vector<float> parameters);  // unordered_map: <reg, param(s)>

/** save_configs
    Function will save the config parameters to a output config YAML file

    @param output_config Type std::string output_config
    @return void
*/
static void save_configs(std::string output_config) {
    std::cout << "here" << std::endl;
    YAML::Node config_backup;  // starts out as null

    config_backup["TE_dir"] = trading_economics_dir;
    config_backup["regions"] = regions;
    config_backup["sectors"] = sectors;
    config_backup["years_to_observe"] = years_to_observe;
    config_backup["year_validation"] = year_validation;
    config_backup["num_params_per_region"] = num_params_per_region;
    config_backup["params_min"] = params_min;
    config_backup["params_max"] = params_max;

    std::ofstream fout(output_config);
    fout << config_backup;
}

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
            // TODO passage shocks
            // TODO shock
            case settings::hstring::hash("flooding"):
                impact = std::make_unique<impactgen::Flooding>(impact_node, output.prepare_forcing());
                impact_name = "Flooding";
                break;
            case settings::hstring::hash("tropical_cyclones"):
                impact = std::make_unique<impactgen::TropicalCyclones>(impact_node, output.prepare_forcing());
                impact_name = "Tropical Cyclones";
                break;
            case settings::hstring::hash("heat_labor_productivity"):
                impact = std::make_unique<impactgen::HeatLaborProductivity>(impact_node, output.prepare_forcing());
                impact_name = "Heat Labor Productivity";
                break;
            default:
                throw std::runtime_error("Unsupported impact type '" + std::string(type) + "'");
        }
        std::size_t combination_count = 1;
        std::unordered_map<std::string, std::tuple<int, int, int>> range_variables;
        std::unordered_map<std::string, std::tuple<std::size_t, std::vector<std::string>>> sequence_variables;
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
                        throw std::runtime_error("Variable '" + key + "' not found for '" + temp  // NOLINT(performance-inefficient-string-concatenation)
                                                 + "'");
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
                 "         Jiacheng Yao <jiacheng@pik-potsdam.de>\n"
                 "\n"
                 "Usage:   "
              << program_name
              << " (<option> | <settingsfile>)\n"
                 "Options:\n"
#ifdef IMPACTGEN_HAS_DIFF
                 "  -d, --diff     Print git diff output from compilation\n"
#endif
                 "  -h, --help     Print this help text\n"
                 "  -c, --calibration     Calibration Mode\n"
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
                run(settings::SettingsNode(std::make_unique<settings::YAML>(std::cin)));
            } else if (arg == "--calibration" || arg == "-c") {
                // If calibration mode, we won't need to input settingsfile, but generate settings
                // either randomly, or based on result of Bayesian Optimization,
                // currently generated only randomly

                // initialize output data
                std::unordered_map<std::string, std::vector<float>> trading_economics_data;
                initialize_te_data(trading_economics_data);

                // initialize times data
                std::vector<TimeRange> times;
                initialize_times(times);

                // int num_calibration_iters = 100000;
                // float min_loss_val = 10000.0;
                // // 1. loop through num_calibration_iters
                // for (int i = 0; i < num_calibration_iters; i++)
                // {
                //     // 2. generate random parameters,
                //     std::unordered_map<std::string, std::vector<float>> parameters;
                //     initialize_parameters(parameters);
                //     // TODO: 3. calculate grid-wise damages/production losses
                //     // TODO: 4. reaggregate damages/production losses to regional level and save in:
                //     // std::unordered_map<std::string, std::vector<float>> model_forecast_data;
                //     // 5: calculate loss_value under the current parameter condition
                //     // float tmp_loss = loss_value(trading_economics_data, model_forecast_data);
                //     if (tmp_loss < min_loss_val)
                //     {
                //         min_loss = tmp_loss;
                //         // TODO: 6: save current parameters as params_optimal
                //     }
                // }
                // TODO: 7: save params_optimal to file

                // save config parameters
                // save_configs("config_backup_test.yaml");
            } else {
                std::ifstream settings_file(arg);
                if (!settings_file) {
                    throw std::runtime_error("Cannot open " + arg);
                }
                run(settings::SettingsNode(std::make_unique<settings::YAML>(settings_file)));
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
