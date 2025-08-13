#pragma once

#include "times.h"

#include <iostream>
#include <string>

struct Candle {
  LocalTimePoint datetime;
  double open = 0.0;
  double high = 0.0;
  double low = 0.0;
  double close = 0.0;
  int volume = 0.0;

  std::string day() const { return std::format("{:%F}", time()); }
  double price() const { return close; }
  LocalTimePoint time() const { return datetime; }
};

using TimeSeriesRes = std::vector<Candle>;
using RealTimeRes = std::pair<Candle, Candle>;
