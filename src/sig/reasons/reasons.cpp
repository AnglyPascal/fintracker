#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

Reason ema_crossover_entry(const IndicatorsTrends& ind, int idx);
Reason rsi_cross_50_entry(const IndicatorsTrends& m, int idx);
Reason pullback_bounce_entry(const IndicatorsTrends& ind, int idx);
Reason macd_histogram_cross_entry(const IndicatorsTrends& ind, int idx);
Reason broke_resistance_entry(const IndicatorsTrends& ind, int idx);

Reason ema_crossdown_exit(const IndicatorsTrends& ind, int idx);
Reason macd_bearish_cross_exit(const IndicatorsTrends& ind, int idx);
Reason broke_support_exit(const IndicatorsTrends& ind, int idx);

inline constexpr signal_f reason_funcs[] = {
    // Entry
    ema_crossover_entry,
    rsi_cross_50_entry,
    pullback_bounce_entry,
    macd_histogram_cross_entry,
    broke_resistance_entry,
    // Exit
    ema_crossdown_exit,
    macd_bearish_cross_exit,
    broke_support_exit,
};

// Check for conflicting reasons and apply penalties
inline void check_conflicts(std::vector<Reason>& reasons) {
  if (reasons.size() < 2)
    return;

  bool has_entry = false, has_exit = false;
  for (const auto& r : reasons) {
    has_entry |= r.cls() == SignalClass::Entry;
    has_exit |= r.cls() == SignalClass::Exit;
  }

  // Only penalize if we have conflicting classes
  if (!has_entry || !has_exit)
    return;

  // Check for specific same-indicator conflicts
  bool ema_conflict = false, macd_conflict = false;

  for (const auto& r : reasons) {
    if (r.type == ReasonType::EmaCrossover && has_exit)
      ema_conflict = true;
    if (r.type == ReasonType::EmaCrossdown && has_entry)
      ema_conflict = true;
    if (r.type == ReasonType::MacdBullishCross && has_exit)
      macd_conflict = true;
    if (r.type == ReasonType::MacdBearishCross && has_entry)
      macd_conflict = true;
  }

  // More lenient penalties
  double base_penalty = 0.85;      // Light general conflict penalty
  double specific_penalty = 0.75;  // Medium same-indicator conflict penalty

  for (auto& r : reasons) {
    // General cross-class conflict penalty
    r.score *= base_penalty;

    // Add conflict description
    if (!r.desc.empty())
      r.desc += ", ";
    r.desc += "conflicted";

    // Additional penalty for same-indicator conflicts
    bool is_ema = (r.type == ReasonType::EmaCrossover ||
                   r.type == ReasonType::EmaCrossdown);
    bool is_macd = (r.type == ReasonType::MacdBullishCross ||
                    r.type == ReasonType::MacdBearishCross);

    if ((is_ema && ema_conflict) || (is_macd && macd_conflict)) {
      r.score *= specific_penalty;  // Now total: 0.85 * 0.75 = ~0.64
    }

    // Pullback bounce with exit signals - moderate penalty
    if (r.type == ReasonType::PullbackBounce && has_exit) {
      r.score *= 0.6;  // Still notable but not devastating
      r.desc += ", exit conflict";
    }

    // S/R breaks get minimal penalties (they're strong signals)
    if (r.type == ReasonType::BrokeResistance ||
        r.type == ReasonType::BrokeSupport) {
      r.score /= base_penalty;  // Remove base penalty
      r.score *= 0.95;          // Very light penalty
    }
  }
}

std::vector<Reason> reasons(const IndicatorsTrends& ind, int idx) {
  std::vector<Reason> res;
  for (auto f : reason_funcs) {
    auto reason = f(ind, idx);
    if (reason.type != ReasonType::None) {
      res.push_back(reason);
    }
  }
  check_conflicts(res);
  return res;
}

std::map<ReasonType, SignalStats> Stats::get_reason_stats(const Backtest& bt) {
  std::map<ReasonType, SignalStats> reason;
  for (auto& f : reason_funcs) {
    auto [r, s] = bt.get_stats<ReasonType>(f);
    reason.try_emplace(r, s);
  }
  return reason;
}
