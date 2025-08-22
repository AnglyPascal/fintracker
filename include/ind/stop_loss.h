#include "indicators.h"

enum class StopOrderType {
  STOP_LOSS,  // Regular stop loss (guaranteed execution)
  STOP_LIMIT  // Stop limit (better price, execution risk)
};

struct StopLoss {
  double swing_low = 0.0;
  double ema_stop = 0.0;
  double atr_stop = 0.0;
  double final_stop = 0.0;
  double stop_pct = 0.0;
  bool is_trailing = false;

  // New fields for stop-limit functionality
  bool use_stop_limit = false;
  double limit_price = 0.0;
  double limit_pct = 0.0;
  StopOrderType order_type = StopOrderType::STOP_LOSS;

  StopLoss() noexcept = default;
  StopLoss(const Metrics& m) noexcept;

 private:
  double calculate_limit_price(const Metrics& m, double stop_price) const;
};

enum class TargetType {
  RESISTANCE_BASED,  // Target at next strong resistance
  PERCENTAGE_BASED,  // Target at risk-reward multiple
  HYBRID             // Whichever is closer/more conservative
};

struct ProfitTarget {
  double target_price = 0.0;
  double target_pct = 0.0;
  double risk_reward_ratio = 0.0;
  TargetType type;

  double resistance_conf = 0.0;
  minutes resistance_inv = H_1;

  ProfitTarget() noexcept = default;
  ProfitTarget(const Metrics& m, const StopLoss& stop_loss) noexcept;

 private:
  std::pair<Zone, minutes> calculate_resistance_target(const Metrics& m) const;
  double calculate_percentage_target(double entry_price,
                                     double stop_price) const;
};

