#include "calendar.h"
#include "times.h"

#include <fstream>
#include <iostream>

Calendar::Calendar() {
  std::ifstream file("data/calendar.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string ev_type, symbol, date;

    if (std::getline(ss, ev_type, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, date)) {
      char type;
      if (ev_type == "Earnings")
        type = 'E';
      else if (ev_type == "Dividends")
        type = 'D';
      else
        type = 'S';

      events[symbol].emplace_back(type, date_to_local(date));
    }
  }

  for (auto& [s, arr] : events)
    std::sort(arr.begin(), arr.end(),
              [](auto& l, auto& r) { return l.ny_date < r.ny_date; });
}

Event Calendar::next_event(std::string symbol) const {
  auto it = events.find(symbol);
  if (it == events.end() || it->second.empty())
    return {};

  auto& event = it->second[0];
  auto diff_days = duration_cast<days>(event.ny_date - now_ny_time()).count();
  if (diff_days < 0)
    return {};

  return event;
}
