#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>

using namespace std;

std::vector<std::string> regions = { "USA", "CHN", "JPN", "DEU", "GBR",
                                     "FRA", "IND", "ITA", "BRA", "CAN",
                                     "KOR", "RUS", "ESP", "AUS", "MEX",
                                     "IDN", "TUR", "NLD", "CHE", "SAU",
                                     "ARG", "ZAF", "SGP", "THA"};
struct TimeRange {
  int begin;
  int count;
};
std::vector<TimeRange> times = { {0, 31}, {31 + 28, 28} };

int event_hurricane_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0}; // for Hurricane: Aug, Sep, Oct
int event_heatstress_months_to_observe[12] = {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0}; // for Heatstress: Jun, Jul, Aug
int event_flooding_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0}; // for Flooding: Aug, Sep, Oct
int years_to_observe[10] = {2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009};
int year_validation = 2010;

int tmp_idx = 0;
for (int i = 0; i < 10; ++i)
{
  for (int j = 0; j < 12; ++i)
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


void initialize_impactgen(settings::SettingsNode& settings,
  std::vector<TimeRange> times,
  std::unordered_map<std::string,
  std::vector<float> trading_economics_data);
float generate_impact(std::vector<float> parameters);

int main()
{
  // TO DO:
  // define settings, map, trading_economics_data and parameters
  initialize_impactgen(settings, times, map, trading_economics_data);
  generate_impact(parameters);
  return 0;
}
