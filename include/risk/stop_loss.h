#pragma once

#include "ind/indicators.h"
#include "sig/signal_types.h"

enum class MarketRegime {
  TRENDING_UP,
  CHOPPY_BULLISH,
  RANGE_BOUND,
  CHOPPY_BEARISH,
  TRENDING_DOWN
};

struct StopLoss {
  // Core stop values
  double initial_stop = 0.0;   // Wider stop for first 2 days
  double standard_stop = 0.0;  // Normal stop after initial period
  double current_stop = 0.0;   // Actual stop based on days held

  // Components used
  double atr_stop = 0.0;
  double support_stop = 0.0;

  // Metadata
  double stop_distance = 0.0;   // In price units
  double stop_pct = 0.0;        // As percentage
  double atr_multiplier = 0.0;  // Multiplier used

  // Trailing stop info
  bool is_trailing = false;
  double trailing_activation_price = 0.0;
  double trailing_stop = 0.0;

  // Decision tracking
  int days_held = 0;
  MarketRegime regime = MarketRegime::RANGE_BOUND;

  std::string rationale = "";

  StopLoss() noexcept = default;
  StopLoss(const Metrics& m,
           LocalTimePoint tp,
           MarketRegime regime,
           int days_held = 0) noexcept;

 private:
  double calculate_atr_multiplier(double daily_atr_pct) const;
  double calculate_support_buffer(double daily_atr_pct) const;
  double calculate_regime_adjustment(MarketRegime regime) const;
  double calculate_time_adjustment(int days_held) const;
  double calculate_trailing_adjustment(int days_to_1r) const;
};

StopHit calculate_stop_hit(const Metrics& m, const StopLoss& sl);
