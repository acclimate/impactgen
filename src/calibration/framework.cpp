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

int event_hurricane_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0};
int event_heatstress_months_to_observe[12] = {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0};
int event_flooding_months_to_observe[12] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0};

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
