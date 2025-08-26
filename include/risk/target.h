#pragma once

#include "ind/indicators.h"
#include "sizing.h"
#include "stop_loss.h"

enum class TargetStrategy {
  CONSERVATIVE,  // 1.5:1 R:R
  STANDARD,      // 2.0:1 R:R
  AGGRESSIVE     // 3.0:1 R:R
};

struct ProfitTarget {
  // Primary targets
  double initial_target = 0.0;
  double stretch_target = 0.0;
  double target_price = 0.0;  // Actual target we're using

  // Risk/Reward
  double risk_reward_ratio = 0.0;
  double target_pct = 0.0;

  // Strategy selection
  TargetStrategy strategy = TargetStrategy::STANDARD;

  // Resistance levels found
  double nearest_resistance = 0.0;
  double resistance_room_pct = 0.0;
  minutes resistance_timeframe = H_1;

  // ATR projection
  double atr_projection = 0.0;
  int expected_days_to_target = 0;

  std::string rationale = "";

  ProfitTarget() noexcept = default;
  ProfitTarget(const Metrics& m,
               LocalTimePoint tp,
               const CombinedSignal& signal,
               const StopLoss& stop_loss,
               const PositionSizing& sizing) noexcept;

 private:
  TargetStrategy select_strategy(const CombinedSignal& signal,
                                 double resistance_room) const;
  double calculate_atr_projection(const Metrics& m,
                                  LocalTimePoint tp,
                                  int days) const;
};
