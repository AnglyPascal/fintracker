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
  Recommendation rec = Recommendation::Avoid;

  double rec_shares = 0.0;
  double rec_capital = 0.0;

  double risk = 0.0;
  double risk_pct = 0.0;
  double risk_amount = 0.0;

  std::string rationale = "";

  PositionSizing() = default;
  PositionSizing(const Metrics& metrics,
                 const CombinedSignal& signal,
                 const StopLoss& stop_loss);
};
