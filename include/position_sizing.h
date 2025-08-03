#pragma once

#include "config.h"
#include "indicators.h"

struct TimeframeRisk {
  double volatility_score = 0.0;  // Based on ATR relative to price
  double trend_alignment = 0.0;   // How aligned trends are
  Rating signal_strength = Rating::None;

  TimeframeRisk() = default;
  TimeframeRisk(const Indicators& ind);
};

enum class Recommendation {
  StrongBuy,  // Full position size
  Buy,        // 75% of max size
  WeakBuy,    // 50% of max size
  Caution,    // 25% of max size
  Avoid       // No position
};

struct PositionSizingConfig;

struct PositionSizing {
  double recommended_shares = 0.0;
  double recommended_capital = 0.0;

  double actual_risk_pct = 0.0;
  double actual_risk_amount = 0.0;

  TimeframeRisk risk_1h, risk_4h, risk_1d;
  double overall_risk_score = 0.0;

  Recommendation recommendation = Recommendation::Avoid;

  // Validation flags
  bool is_valid = false;
  std::vector<std::string> warnings;

  PositionSizing() = default;
  PositionSizing(const Metrics& metrics,
                 const CombinedSignal& signal,
                 const StopLoss& stop_loss,
                 const PositionSizingConfig& config);

  // Helper methods
  bool meets_minimum_criteria() const;
  double calculate_position_value() const {
    return recommended_shares * get_current_price();
  }

 private:
  double current_price = 0.0;
  double get_current_price() const { return current_price; }

  // Calculate overall risk score (0-10, where 10 is highest risk)
  double calculate_overall_risk(const Metrics& metrics);

  // Determine position size multiplier based on risk and signal quality
  double get_size_multiplier(const CombinedSignal& signal, double risk_score);
};

