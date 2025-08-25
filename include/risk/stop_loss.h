#pragma once

#include "ind/indicators.h"

struct StopLoss {
  double swing_low = 0.0;
  double atr_stop = 0.0;
  double final_stop = 0.0;
  double stop_pct = 0.0;
  bool is_trailing = false;

  std::string rationale = "";

  StopLoss() noexcept = default;
  StopLoss(const Metrics& m) noexcept;
};
