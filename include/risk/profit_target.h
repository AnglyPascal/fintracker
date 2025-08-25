#pragma once

#include "ind/indicators.h"
#include "stop_loss.h"

enum class TargetType { RESISTANCE_BASED, PERCENTAGE_BASED, HYBRID };

struct ProfitTarget {
  double target_price = 0.0;
  double target_pct = 0.0;
  double risk_reward_ratio = 0.0;
  TargetType type;

  double resistance_conf = 0.0;
  minutes resistance_inv = H_1;

  std::string rationale = "";

  ProfitTarget() noexcept = default;
  ProfitTarget(const Metrics& m, const StopLoss& stop_loss) noexcept;

 private:
  std::pair<Zone, minutes> calculate_resistance_target(const Metrics& m) const;
  double calculate_percentage_target(double entry_price,
                                     double stop_price) const;
};
