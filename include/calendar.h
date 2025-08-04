#pragma once

#include "times.h"

#include <string>
#include <unordered_map>
#include <vector>

struct Event {
  char type = '\0';
  LocalTimePoint ny_date;
  int days_until() const;
};

std::vector<Event> get_events();

struct Calendar {
  std::unordered_map<std::string, std::vector<Event>> events;

  Calendar();
  Event next_event(std::string symbol) const;
};
