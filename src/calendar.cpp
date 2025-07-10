#include "calendar.h"

#include <chrono>
#include <fstream>
#include <iostream>

inline SysTimePoint to_timepoint(const std::string& date_str) {
  std::istringstream in{date_str};
  std::chrono::sys_days days;
  in >> std::chrono::parse("%F", days);  // %F = YYYY-MM-DD
  if (in.fail())
    return {};
  return days;
}

std::vector<Event> get_events() {
  std::vector<Event> events;

  std::ifstream file("data/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string ev_type, symbol, date;

    if (std::getline(ss, date, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, ev_type)) {
      EventType type;
      if (ev_type == "Earnings")
        type = EventType::Earnings;
      else if (ev_type == "Dividends")
        type = EventType::Dividends;
      else
        type = EventType::Splits;

      events.emplace_back(type, symbol, to_timepoint(date));
    }
  }

  return events;
}
