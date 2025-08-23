#include "ind/calendar.h"
#include "util/times.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <glaze/glaze.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_set>

using json = nlohmann::json;
using namespace std::chrono;

int Event::days_until() const {
  auto date_now = floor<days>(now_ny_time());
  if (ny_date < date_now)
    return -1;
  return duration_cast<days>(ny_date - date_now).count();
}

template <>
struct glz::meta<LocalTimePoint> {
  using T = LocalTimePoint;

  static constexpr auto write = [](const T& time_point) {
    return std::format("{:%F %T}", time_point);
  };

  static constexpr auto read = [](T& t, const std::string& str) {
    t = datetime_to_local(str);
  };

  static constexpr auto value = custom<read, write>;
};

Calendar::Calendar(const Symbols& symbols) {
  if (weekday{floor<days>(now_ny_time())} == Monday) {
    fetch_events(symbols);
    return;
  }

  auto ec = glz::read_file_json(events, filename, std::string{});
  if (ec)
    spdlog::error("[calendar] error reading {}", filename.c_str());

  for (auto& [s, arr] : events)
    std::sort(arr.begin(), arr.end(),
              [](auto& l, auto& r) { return l.ny_date < r.ny_date; });
}

inline auto fetch_nasdaq_events(auto& event_type, auto& date) {
  Events events;

  auto url = std::format("https://api.nasdaq.com/api/calendar/{}?date={}",
                         event_type, date);
  cpr::Response r =
      cpr::Get(cpr::Url{url}, cpr::Header{{"User-Agent", "Mozilla/5.0"}});

  if (r.status_code != 200) {
    std::cerr << std::format("Failed to fetch {} for {}: HTTP {}\n",  //
                             event_type, date, r.status_code);
    return events;
  }

  try {
    auto root = json::parse(r.text);
    auto data = root["data"];
    if (!data.is_object())
      return events;

    if (event_type == "earnings") {
      auto earnings = data["rows"];
      for (auto& item : earnings) {
        auto sym = item.value("symbol", "");
        events[sym].emplace_back('E', datetime_to_local(date));
      }
    } else if (event_type == "dividends") {
      auto dividends = data["calendar"]["rows"];
      for (auto& item : dividends) {
        auto sym = item.value("symbol", "");
        events[sym].emplace_back('D', datetime_to_local(date));
      }
    } else if (event_type == "splits") {
      auto splits = data["rows"];
      for (auto& item : splits) {
        auto sym = item.value("symbol", "");
        events[sym].emplace_back('S', datetime_to_local(date));
      }
    }
  } catch (const std::exception& e) {
    std::cerr << std::format("JSON parse error for {} on {}: {}\n",  //
                             event_type, date, e.what());
  }
  return events;
}

void Calendar::fetch_events(const Symbols& symbols) {
  std::unordered_set<std::string> watchlist;
  for (auto& si : symbols.arr)
    watchlist.insert(si.symbol);

  auto today = floor<days>(now_ny_time());

  for (int i = 0; i < 30; ++i) {
    auto day = today + std::chrono::hours(24 * i);
    auto date = datetime_to_string(day);

    for (const auto& event_type : {"earnings", "dividends", "splits"}) {
      auto events = fetch_nasdaq_events(event_type, date);
      for (auto& [sym, evs] : events) {
        if (watchlist.count(sym) == 0)
          continue;
        auto& arr = events[sym];
        arr.insert(arr.end(), evs.begin(), evs.end());
      }
    }
  }

  for (auto& [s, arr] : events)
    std::sort(arr.begin(), arr.end(),
              [](auto& l, auto& r) { return l.ny_date < r.ny_date; });

  auto ec = glz::write_file_json(events, filename, std::string{});
  if (ec)
    spdlog::error("[calendar] error writing {}", filename.c_str());
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
