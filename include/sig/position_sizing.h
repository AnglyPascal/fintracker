#pragma once

#include "ind/indicators.h"

struct Indicators;
struct Metrics;
struct StopLoss;

enum class Recommendation {
  StrongBuy,  // Full position size
  Buy,        // 75% of max size
  WeakBuy,    // 50% of max size
  Caution,    // 25% of max size
  Avoid       // No position
};

struct PositionSizing {
  double rec_shares = 0.0;
  double rec_capital = 0.0;

  double risk = 0.0;
  double risk_pct = 0.0;
  double risk_amount = 0.0;

  Recommendation rec = Recommendation::Avoid;
  std::vector<std::string> warnings;

  PositionSizing() = default;
  PositionSizing(const Metrics& metrics,
                 const CombinedSignal& signal,
                 const StopLoss& stop_loss);

  bool meets_minimum_criteria() const;
  double calculate_position_value() const {
    return rec_shares * get_current_price();
  }

 private:
  double current_price = 0.0;
  double get_current_price() const { return current_price; }
};
