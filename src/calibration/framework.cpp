#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>

struct TimeRange {
    int begin;
    int length;
};

std::vector<std::string> regions = { "USA", "CHN", "JPN", "DEU", "GBR",
                                     "FRA", "IND", "ITA", "BRA", "CAN",
                                     "KOR", "RUS", "ESP", "AUS", "MEX",
                                     "IDN", "TUR", "NLD", "CHE", "SAU",
                                     "ARG", "ZAF", "SGP", "THA"};
struct IndexRange {
  int begin;
  int count;
};
std::vector<IndexRange> times = { {0, 31}, {31 + 28, 28} };

int event_hurricane_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0}; // for Hurricane: Aug, Sep, Oct
int event_heatstress_months_to_observe[12] = {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0}; // for Heatstress: Jun, Jul, Aug
int event_flooding_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0}; // for Flooding: Aug, Sep, Oct
int years_to_observe[10] = {2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009};
int year_validation = 2010;

void check_leap_year(year)
{
  if ( ( (year%4==0) && (year%100!=0) ) || (year%400==0) )
​  {
    return true;
  }
  else
  {
    return false;
  }
}

void initialize_impactgen(settings::SettingsNode& settings,
  std::vector<TimeRange> times,
  std::unordered_map<std::string,
  std::vector<float> trading_economics_data);
float generate_impact(std::vector<float> parameters);

int main()
{
  return 0;
}
