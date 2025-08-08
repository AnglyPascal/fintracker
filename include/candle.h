#pragma once

#include "times.h"

#include <string>

struct Candle {
  std::string datetime = "";
  double open = 0.0;
  double high = 0.0;
  double low = 0.0;
  double close = 0.0;
  int volume = 0.0;

  std::string day() const { return datetime.substr(0, 10); }
  double price() const { return close; }
  LocalTimePoint time() const { return datetime_to_local(datetime); }
};
