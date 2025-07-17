#pragma once

#include "signal.h"

#include <unordered_map>

struct SignalStats {
  std::unordered_map<ReasonType, double> reason_perf;
  std::unordered_map<HintType, double> hint_perf;
};
