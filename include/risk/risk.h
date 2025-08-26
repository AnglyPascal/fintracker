#pragma once

#include "core/positions.h"
#include "sig/combined_signal.h"
#include "sizing.h"
#include "stop_loss.h"
#include "target.h"

struct ScalingRules {
  // Scale up
  bool can_add = false;
  double add_trigger_price = 0.0;
  double add_size_shares = 0.0;

  // Scale down
  bool should_trim = false;
  double trim_trigger_price = 0.0;
  double trim_size_shares = 0.0;

  std::string rationale = "";

  ScalingRules() noexcept = default;
  ScalingRules(const Metrics& m,
               LocalTimePoint tp,
               const StopLoss& stop_loss,
               const PositionSizing& initial_sizing);
};

class Risk {
  MarketRegime regime;

 public:
  StopLoss stop_loss;
  PositionSizing sizing;
  ProfitTarget target;
  ScalingRules scaling;

  // Overall assessment
  bool should_take_trade = false;
  bool is_near_earnings = false;
  std::string overall_rationale = "";

  Risk() noexcept = default;
  Risk(const Metrics& m,
       const Indicators& spy,
       LocalTimePoint tp,
       const CombinedSignal& signal,
       const OpenPositions& positions,
       const Event& next_event) noexcept;

  // Convenience getters
  double get_risk_reward() const { return target.risk_reward_ratio; }
  double get_position_risk() const { return sizing.position_risk_pct; }
  bool can_take_position() const { return should_take_trade; }
  MarketRegime get_regime() const { return regime; }

  StopHit get_stop_hit(const Metrics& m) const {
    return calculate_stop_hit(m, stop_loss);
  }

 private:
  MarketRegime detect_market_regime(const Indicators& spy, int idx) const;
  bool check_earnings_proximity(const Event& event) const;
};
