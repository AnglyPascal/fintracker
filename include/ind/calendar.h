#pragma once

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

std::vector<Event> get_events();

struct Calendar {
  std::unordered_map<std::string, std::vector<Event>> events;

  Calendar();
  Event next_event(std::string symbol) const;
};
