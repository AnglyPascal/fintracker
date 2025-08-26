#pragma once

#include "filters.h"
#include "signals.h"

struct StopLoss;
struct Event;

struct CombinedSignal {
  Rating type = Rating::None;
  Score score;
  std::string rationale;

  StopHit stop_hit;
  Filters filters;
  Forecast forecast;

  CombinedSignal() = default;
  CombinedSignal(const Metrics& m, const Event& ev, LocalTimePoint tp = {});

  void apply_stop_hit(StopHit hit);
};

