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
  auto today = today_ny();
  return date < today ? -1 : duration_cast<days>(date - today).count();
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

inline auto fetch_nasdaq_events(auto& event_type, auto& date) {
  EventsMap events;

  auto url = std::format("https://api.nasdaq.com/api/calendar/{}?date={:%F}",
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
        events[sym].emplace('E', date);
      }
    } else if (event_type == "dividends") {
      auto dividends = data["calendar"]["rows"];
      for (auto& item : dividends) {
        auto sym = item.value("symbol", "");
        events[sym].emplace('D', date);
      }
    } else if (event_type == "splits") {
      auto splits = data["rows"];
      for (auto& item : splits) {
        auto sym = item.value("symbol", "");
        events[sym].emplace('S', date);
      }
    }
  } catch (const std::exception& e) {
    std::cerr << std::format("JSON parse error for {} on {}: {}\n",  //
                             event_type, date, e.what());
  }
  return events;
}

Calendar::Calendar(const Symbols& symbols) {
  auto ec = glz::read_file_json(events, filename, std::string{});
  if (ec) {
    events.from = today_ny();
    events.to = events.from + days{30};
    events.data = fetch_events(symbols, events.from, events.to);
    backup();
    return;
  }

  auto today = today_ny();
  if (today <= events.from + days{7})
    return;

  for (auto [sym, arr] : events.data)
    arr.erase(arr.begin(), arr.lower_bound({'\0', today}));

  auto mp = fetch_events(symbols, events.to, events.to + days{7});
  for (auto& [sym, arr] : mp)
    events.data[sym].insert(arr.begin(), arr.end());

  events.from = today;
  events.to = events.to + days{7};

  backup();
}

EventsMap Calendar::fetch_events(const Symbols& symbols,
                                 LocalTimePoint from,
                                 LocalTimePoint to) const {
  EventsMap mp;

  std::unordered_set<std::string> watchlist;
  for (auto& si : symbols.arr)
    watchlist.insert(si.symbol);

  for (auto date = from; date <= to; date += days{1}) {
    for (auto& event_type : {"earnings", "dividends", "splits"}) {
      auto events_map = fetch_nasdaq_events(event_type, date);
      for (auto& [sym, evs] : events_map) {
        if (!watchlist.contains(sym))
          continue;
        auto& arr = mp[sym];
        arr.insert(evs.begin(), evs.end());
      }
    }
  }

  return mp;
}

void Calendar::backup() const {
  auto ec = glz::write_file_json(events, filename, std::string{});
  if (ec)
    spdlog::error("[calendar] error writing {}", filename.c_str());
}

Event Calendar::next_event(std::string symbol) const {
  auto it = events.data.find(symbol);
  if (it == events.data.end() || it->second.empty())
    return {};

  auto ev_it = it->second.lower_bound({'\0', today_ny()});
  return ev_it != it->second.end() ? *ev_it : Event{};
}
