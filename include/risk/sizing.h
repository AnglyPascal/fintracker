#pragma once

#include "core/positions.h"
#include "ind/indicators.h"
#include "sig/combined_signal.h"
#include "stop_loss.h"

enum class Recommendation {
  StrongBuy,  // 0.7-1.0% risk
  Buy,        // 0.5-0.7% risk
  WeakBuy,    // 0.3-0.5% risk
  Avoid       // No position
};

struct PositionSizing {
  Recommendation rec = Recommendation::Avoid;

  // Position details
  double rec_shares = 0.0;
  double rec_capital = 0.0;

  // Risk metrics
  double position_risk_pct = 0.0;  // Actual risk for this position
  double position_risk_amount = 0.0;
  double portfolio_risk_after = 0.0;  // Total risk if we take this

  // Constraints applied
  bool was_reduced_for_correlation = false;
  bool was_reduced_for_volatility = false;
  bool was_reduced_for_portfolio_limit = false;
  bool hit_daily_limit = false;

  // Metadata
  double correlation_to_spy = 0.0;
  double daily_atr_pct = 0.0;
  int similar_positions = 0;

  std::string rationale = "";

  PositionSizing() = default;
  PositionSizing(const Metrics& m,
                 const Indicators& spy,
                 LocalTimePoint tp,
                 const CombinedSignal& signal,
                 const StopLoss& stop_loss,
                 const OpenPositions& positions,
                 MarketRegime regime);
};
