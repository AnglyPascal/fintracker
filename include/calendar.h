#pragma once

#include "times.h"

#include <string>
#include <vector>

enum class EventType {
  Earnings,
  Dividends,
  Splits,
};

struct Event {
  EventType type;
  std::string symbol;
  SysTimePoint date;
};

std::vector<Event> get_events();
