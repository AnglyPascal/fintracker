#include "sig/position_sizing.h"
#include "ind/stop_loss.h"
#include "util/config.h"

#include <spdlog/spdlog.h>

inline auto& sizing_config = config.sizing_config;

struct TimeframeRisk {
  double volatility_score = 0.0;  // Based on ATR relative to price
  double trend_alignment = 0.0;   // How aligned trends are

  TimeframeRisk(const Indicators& ind) {
    auto atr = ind.atr(-1);
    auto price = ind.price(-1);
    volatility_score = (atr / price) * 100.0;

    auto ema9 = ind.ema9(-1);
    auto ema21 = ind.ema21(-1);
    auto ema50 = ind.ema50(-1);

    double alignment_score = 0;
    if (ema9 > ema21)
      alignment_score++;
    if (ema21 > ema50)
      alignment_score++;
    if (price > ema9)
      alignment_score++;

    trend_alignment = alignment_score / 3.0;
  }
};

inline double round_to(double x, int n = 2) {
  double factor = std::pow(10.0, n);
  return std::round(x * factor) / factor;
}

// Calculate overall risk score (0-10, where 10 is highest risk)
inline double calculate_risk(auto& ind_1h, auto& ind_4h, auto& ind_1d) {
  TimeframeRisk risk_1h{ind_1h};
  TimeframeRisk risk_4h{ind_4h};
  TimeframeRisk risk_1d{ind_1d};

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
  for (auto r : {ind_1h.signal.type, ind_4h.signal.type, ind_1d.signal.type}) {
    if (r == Rating::Mixed || r == Rating::Caution ||
        r == Rating::HoldCautiously || r == Rating::Exit) {
      conflicting_signals++;
    }
  }
  risk_score += conflicting_signals;

  return std::min(risk_score, 10.0);
}

// Determine position size multiplier based on risk and signal quality
inline double get_size_multiplier(const CombinedSignal& signal,
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

  // Adjust based on forecast conf
  if (signal.forecast.conf > 0.8)
    base_multiplier *= 1.1;
  else if (signal.forecast.conf < 0.4)
    base_multiplier *= 0.7;

  // Adjust based on risk score
  double risk_adjustment = 1.0 - (risk_score / 20.0);

  // Never go below 20% of base
  base_multiplier *= std::max(risk_adjustment, 0.2);

  // // Adjust based on number of confirmations
  // if (signal.confs.size() >= 3)
  //   base_multiplier *= 1.05;
  // else if (signal.confs.empty())
  //   base_multiplier *= 0.85;

  return std::clamp(base_multiplier, 0.1, 1.0);
}

PositionSizing::PositionSizing(const Metrics& m,
                               const CombinedSignal& sig,
                               const StopLoss& sl) {
  auto current_price = m.last_price();
  risk = calculate_risk(m.ind_1h, m.ind_4h, m.ind_1d);

  double risk_per_share = current_price - sl.final_stop;
  risk_pct = risk_per_share / current_price;

  double base_shares = sizing_config.max_risk_amount() / risk_per_share;
  double size_multiplier = get_size_multiplier(sig, risk);
  rec_shares = round_to(base_shares * size_multiplier, 2);

  rec_capital = rec_shares * current_price;
  auto max_position_amount = sizing_config.max_position_amount();
  if (rec_capital > max_position_amount) {
    rec_shares = round_to(max_position_amount / current_price, 2);
    rec_capital = rec_shares * current_price;
  }

  // Calculate actual risk with final position size
  risk_amount = rec_shares * risk_per_share;
  risk_pct = round_to(risk_amount / sizing_config.capital_usd, 3);

  bool possible_entry = sig.type == Rating::Entry ||
                        (sig.type == Rating::Watchlist &&
                         sig.score > sizing_config.entry_score_cutoff);
  bool maybe_buy = possible_entry && risk <= sizing_config.entry_risk_cutoff &&
                   sig.forecast.conf > sizing_config.entry_conf_cutoff;

  if (!maybe_buy)
    rec = Recommendation::Avoid;
  else if (size_multiplier >= 0.9)
    rec = Recommendation::StrongBuy;
  else if (size_multiplier >= 0.7)
    rec = Recommendation::Buy;
  else if (size_multiplier >= 0.4)
    rec = Recommendation::WeakBuy;
  else
    rec = Recommendation::Caution;
}

