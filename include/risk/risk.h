#pragma once

#include "position_sizing.h"
#include "profit_target.h"
#include "stop_loss.h"

struct Risk {
  StopLoss stop_loss;
  ProfitTarget target;
  PositionSizing sizing;

  Risk(const Metrics& m, const CombinedSignal& sig) noexcept
      : stop_loss{m}, target{m, stop_loss}, sizing{m, sig, stop_loss} {}
};
