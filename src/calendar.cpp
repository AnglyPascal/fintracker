#include "calendar.h"
#include "times.h"

#include <chrono>
#include <fstream>

int Event::days_until() const {
  using namespace std::chrono;
  auto date_now = floor<days>(now_ny_time());
  if (ny_date < date_now)
    return -1;
  return duration_cast<days>(ny_date - date_now).count();
}

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

  for (auto& ev : it->second)
    if (ev.days_until() >= 0)
      return ev;

  return {};
}
