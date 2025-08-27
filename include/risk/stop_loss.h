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

enum class StopContext {
  NEW_POSITION,       // Fresh entry - used for sizing calculations
  EXISTING_INITIAL,   // Position held 0-2 days, wider stops
  EXISTING_STANDARD,  // Position held 3+ days, standard stops
  EXISTING_TRAILING,  // 1R+ profit achieved, trailing stops
  SCALE_UP_ENTRY      // Adding shares to existing position
};

struct StopLoss {
 private:
  StopContext context;

  // Base calculations (shared across contexts)
  double atr_stop_base = 0.0;
  double support_stop_base = 0.0;
  double atr_multiplier = 0.0;

  // Context-specific stop values
  double new_position_stop = 0.0;
  double initial_period_stop = 0.0;
  double standard_period_stop = 0.0;
  double trailing_stop_value = 0.0;
  double scale_up_stop = 0.0;

  // Position state (when applicable)
  double entry_price = 0.0;
  double max_price_seen = 0.0;
  int days_held = 0;
  MarketRegime regime = MarketRegime::RANGE_BOUND;

  int calculate_days_held(LocalTimePoint entry_time,
                          LocalTimePoint current_time) const;

 public:
  // Metadata for display
  std::string rationale = "";

  StopLoss() noexcept = default;
  StopLoss(const Metrics& m,
           LocalTimePoint tp,
           MarketRegime regime,
           StopContext context = StopContext::NEW_POSITION) noexcept;

  // Primary interface - returns stop for current context
  double get_stop_price() const;
  double get_stop_distance(double current_price) const;
  double get_stop_percentage(double current_price) const;

  // Context-specific getters
  double get_new_position_stop() const { return new_position_stop; }
  double get_scale_up_stop() const { return scale_up_stop; }
  double get_trailing_stop() const { return trailing_stop_value; }
  double get_initial_stop() const { return initial_period_stop; }
  double get_standard_stop() const { return standard_period_stop; }

  // State queries
  StopContext get_context() const { return context; }
  bool is_trailing_active() const {
    return context == StopContext::EXISTING_TRAILING;
  }
  bool requires_wider_stop() const {
    return context == StopContext::EXISTING_INITIAL;
  }

  // For sizing calculations
  double get_sizing_stop() const {
    return context == StopContext::NEW_POSITION     ? new_position_stop
           : context == StopContext::SCALE_UP_ENTRY ? scale_up_stop
                                                    : get_stop_price();
  }

  // Legacy compatibility methods
  double standard_distance() const {
    return entry_price - standard_period_stop;
  }
  double initial_distance() const { return entry_price - initial_period_stop; }
  double current_distance() const { return entry_price - get_stop_price(); }
  double current_stop() const { return get_stop_price(); }
  double stop_pct() const { return get_stop_percentage(entry_price); }
};

StopHit calculate_stop_hit(const Metrics& m, const StopLoss& sl);
