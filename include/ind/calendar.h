#pragma once

#include "util/symbols.h"
#include "util/times.h"

#include <set>
#include <string>
#include <unordered_map>

struct Event {
  char type = '\0';
  LocalTimePoint date;

  int days_until() const;

  bool is_earnings() const { return type == 'E'; }
  bool is_dividend() const { return type == 'D'; }

  auto operator<=>(const Event& other) const {
    if (date == other.date)
      return type <=> other.type;
    return date <=> other.date;
  }

  auto operator==(const Event& other) const {
    return type == other.type && date == other.date;
  }
};

using EventsMap = std::unordered_map<std::string, std::set<Event>>;

struct Events {
  LocalTimePoint from;
  LocalTimePoint to;
  EventsMap data;
};

struct Calendar {
  Events events;

  Calendar(const Symbols& symbols);
  Event next_event(std::string symbol) const;

 private:
  std::string filename = "private/calendar.json";

  EventsMap fetch_events(const Symbols& symbols,
                         LocalTimePoint from,
                         LocalTimePoint to) const;
  void backup() const;
};
