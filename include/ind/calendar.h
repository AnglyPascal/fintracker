#pragma once

#include "util/symbols.h"
#include "util/times.h"

#include <string>
#include <unordered_map>
#include <vector>

struct Event {
  char type = '\0';
  LocalTimePoint ny_date;
  int days_until() const;

  bool is_earnings() const { return type == 'E'; }
  bool is_dividend() const { return type == 'D'; }
};

using Events = std::unordered_map<std::string, std::vector<Event>>;

struct Calendar {
  Events events;

  Calendar(const Symbols& symbols);
  Event next_event(std::string symbol) const;

 private:
  std::string filename = "private/calendar.json";

  void fetch_events(const Symbols& symbols);
  void take_backup() const;
};
