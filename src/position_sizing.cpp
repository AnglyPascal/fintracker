#include "position_sizing.h"
#include "config.h"
#include "format.h"

#include <spdlog/spdlog.h>

const PositionSizingConfig& sizing_config = config.sizing_config;

inline double round_to(double x, int n = 2) {
  double factor = std::pow(10.0, n);
  return std::round(x * factor) / factor;
}

PositionSizing::PositionSizing(const Metrics& metrics,
                               const CombinedSignal& signal,
                               const StopLoss& stop_loss) {
  current_price = metrics.last_price();

  risk_1h = TimeframeRisk{metrics.ind_1h};
  risk_4h = TimeframeRisk{metrics.ind_4h};
  risk_1d = TimeframeRisk{metrics.ind_1d};

  overall_risk_score = calculate_overall_risk(metrics);

  double risk_per_share = current_price - stop_loss.final_stop;
  actual_risk_pct = risk_per_share / current_price;

  // Validate minimum risk threshold
  if (actual_risk_pct < 0.005)  // Less than 0.5%
    warnings.push_back("stop loss too tight");

  if (actual_risk_pct > 0.035) {  // More than 3.5%
    warnings.push_back("stop loss too high");
    return;
  }

  // Calculate base position size based on max risk
  double max_risk_amount = sizing_config.max_risk_amount();
  double base_shares = max_risk_amount / risk_per_share;

  // Apply size multiplier based on signal quality and risk
  double size_multiplier = get_size_multiplier(signal, overall_risk_score);
  recommended_shares = round_to(base_shares * size_multiplier, 2);

  // Ensure we don't exceed capital constraints: max 25% of capital per position
  recommended_capital = recommended_shares * current_price;
  if (recommended_capital >
      sizing_config.capital_usd * sizing_config.max_capital_per_position) {
    auto total_capital =
        sizing_config.capital_usd * sizing_config.max_capital_per_position;
    recommended_shares = round_to(total_capital / current_price, 2);
    recommended_capital = recommended_shares * current_price;
  }

  // Calculate actual risk with final position size
  actual_risk_amount = recommended_shares * risk_per_share;
  actual_risk_pct = actual_risk_amount / sizing_config.capital_usd;

  bool possible_entry = signal.type == Rating::Entry ||
                        (signal.type == Rating::Watchlist &&
                         signal.score > sizing_config.entry_score_cutoff);
  bool maybe_buy =
      possible_entry && overall_risk_score <= sizing_config.entry_risk_cutoff &&
      signal.forecast.confidence > sizing_config.entry_confidence_cutoff;

  if (maybe_buy) {
    if (size_multiplier >= 0.9)
      recommendation = Recommendation::StrongBuy;
    else if (size_multiplier >= 0.7)
      recommendation = Recommendation::Buy;
    else if (size_multiplier >= 0.4)
      recommendation = Recommendation::WeakBuy;
    else
      recommendation = Recommendation::Caution;
  } else {
    recommendation = Recommendation::Avoid;
  }

  is_valid = recommended_shares > 0 && recommendation != Recommendation::Avoid;

  spdlog::trace("[pos] {}: {:.2f} w/ {:.2f}", to_str(recommendation).c_str(),
                recommended_shares, recommended_capital);
}

TimeframeRisk::TimeframeRisk(const Indicators& ind) {
  signal_strength = ind.signal.type;

  double atr = ind.atr(-1);
  double price = ind.candles.back().close;
  volatility_score = (atr / price) * 100.0;

  double ema9 = ind.ema9(-1);
  double ema21 = ind.ema21(-1);
  double ema50 = ind.ema50(-1);

  int alignment_score = 0;
  if (ema9 > ema21)
    alignment_score++;
  if (ema21 > ema50)
    alignment_score++;
  if (price > ema9)
    alignment_score++;

  trend_alignment = alignment_score / 3.0;
}

double PositionSizing::calculate_overall_risk(const Metrics&) {
  double risk_score = 0.0;

  // Volatility component (0-4 points)
  double avg_volatility = (risk_1h.volatility_score + risk_4h.volatility_score +
                           risk_1d.volatility_score) /
                          3.0;
  if (avg_volatility > 3.0)
    risk_score += 4.0;
  else if (avg_volatility > 2.0)
    risk_score += 3.0;
  else if (avg_volatility > 1.5)
    risk_score += 2.0;
  else if (avg_volatility > 1.0)
    risk_score += 1.0;

  // Trend alignment component (0-3 points, less alignment = more risk)
  double avg_alignment = (risk_1h.trend_alignment + risk_4h.trend_alignment +
                          risk_1d.trend_alignment) /
                         3.0;
  risk_score += (1.0 - avg_alignment) * 3.0;

  // Signal consistency component (0-3 points)
  int conflicting_signals = 0;
  for (auto rating : {risk_1h.signal_strength, risk_4h.signal_strength,
                      risk_1d.signal_strength}) {
    if (rating == Rating::Mixed || rating == Rating::Caution ||
        rating == Rating::HoldCautiously || rating == Rating::Exit) {
      conflicting_signals++;
    }
  }
  risk_score += conflicting_signals;

  return std::min(risk_score, 10.0);
}

double PositionSizing::get_size_multiplier(const CombinedSignal& signal,
                                           double risk_score) {
  double base_multiplier = 1.0;

  // Adjust based on signal rating
  switch (signal.type) {
    case Rating::Entry:
      base_multiplier = 1.0;
      break;
    case Rating::Watchlist:
      base_multiplier = 0.6;
      break;
    default:
      base_multiplier = 0.3;
      break;
  }

  // Adjust based on forecast confidence
  if (signal.forecast.confidence > 0.8)
    base_multiplier *= 1.1;
  else if (signal.forecast.confidence < 0.4)
    base_multiplier *= 0.7;

  // Adjust based on risk score (0-10 scale, where 10 is highest risk)
  double risk_adjustment = 1.0 - (risk_score / 20.0);

  // Never go below 20% of base
  base_multiplier *= std::max(risk_adjustment, 0.2);

  // Adjust based on number of confirmations
  if (signal.confirmations.size() >= 3)
    base_multiplier *= 1.05;
  else if (signal.confirmations.empty())
    base_multiplier *= 0.85;

  return std::clamp(base_multiplier, 0.1, 1.0);
}

bool PositionSizing::meets_minimum_criteria() const {
  return is_valid && recommendation != Recommendation::Avoid &&
         actual_risk_pct <= 0.02 &&  // Max 2% risk
         recommended_shares > 0;
}
