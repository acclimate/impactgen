#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>

using namespace std;

// NLD: earliest data gathering for NLD is Jan-2001
// this means missing data handling mechanism needs to be in place
// CHE: has only quarterly data starting from Jan-2000.
// SAU: only steel production has monthly data starting from Jan-2000, all other are quarterly or start from 2016

std::vector<std::string> regions = { "USA", "CHN", "JPN", "DEU", "GBR",
                                     "FRA", "IND", "ITA", "BRA", "CAN",
                                     "KOR", "RUS", "ESP", "AUS", "MEX",
                                     "IDN", "TUR", "NLD", "CHE", "SAU",
                                     "ARG", "ZAF", "SGP", "THA"};

// sector preference
std::vector<std::string> sectors = { "Capacity Utilization", "Manufacturing Production",
                                     "Industrial Production", "TREQ", "Steel Production",
                                     "Electricity Production"};

std::string trading_economics_dir = "/p/projects/zeean/calibration/tradingeconomics_data/";

std::unordered_map<std::string, std::vector<float>> trading_economics_data;

for(std::size_t i=0; i<regions.size(); ++i)
{
  for(std::size_t j=0; j<sectors.size(); ++j)
  {
    fstream fileStream;
    std::string tmp_te_file = trading_economics_dir+"production_"+regions[i]+"_"+sectors[j]+".csv";;

    fileStream.open(tmp_te_file);
    if (!fileStream.fail()) { // if the file can be found
      // std::cout << tmp_te_file << '\n';
      // read and parse tmp_te_file as .csv
      // generate tmp_val_vector from tmp_te_file
      trading_economics_data[regions[i]] = tmp_val_vector;
      break;
    }
  }
}

struct TimeRange {
  int begin;
  int count;
};
std::vector<TimeRange> times;

int event_hurricane_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0}; // for Hurricane: Aug, Sep, Oct
int event_heatstress_months_to_observe[12] = {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0}; // for Heatstress: Jun, Jul, Aug
int event_flooding_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0}; // for Flooding: Aug, Sep, Oct
int years_to_observe[10] = {2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009};
int year_validation = 2010;

int tmp_idx = 0;
for (int i = 0; i < 10; ++i)
{
  for (int j = 0; j < 12; ++j)
  {
    int tmp_num_day = get_number_of_days(j, years_to_observe[i]);
    if (event_hurricane_months_to_observe[j]||event_heatstress_months_to_observe[j]||event_flooding_months_to_observe[j])
    {
      times.push_back({tmp_idx+tmp_num_day, tmp_num_day});
    }
    tmp_idx += tmp_num_day;
  }
}


/** get_number_of_days
    Function will return total number of days for month, year combination

    @param month Type int
    @param year Type int
    @return int result
*/
int  get_number_of_days(int month, int year)
{
	//leap year condition, if month is 2
	if( month == 2)
	{
		if((year%400==0) || (year%4==0 && year%100!=0))
			return 29;
		else
			return 28;
	}
	//months which has 31 days
	else if(month == 1 || month == 3 || month == 5 || month == 7 || month == 8
	||month == 10 || month==12)
		return 31;
	else
		return 30;
}

/** check_leap_year
    Function will return true or false of leap year, depending on year

    @param year Type int
    @return boolean result
*/
bool check_leap_year(int year)
{
	if((year%400==0) || (year%4==0 && year%100!=0))
		return true;
	else
		return false;
}


void initialize_impactgen(settings::SettingsNode& settings, // found in main.cpp
  const std::vector<TimeRange>& times,
  std::unordered_map<std::string, std::vector<float>> trading_economics_data);
float generate_impact(std::vector<float> parameters); //unordered_map: <reg, param(s)>

int main()
{
  // TO DO:
  // define settings, map, trading_economics_data and parameters
  initialize_impactgen(settings, times, map, trading_economics_data);
  generate_impact(parameters);
  return 0;
}
