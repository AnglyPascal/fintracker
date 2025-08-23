#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

// Add this new function for multi-timeframe filters
std::vector<Filter> evaluate_timeframe_alignment(const Metrics& m) {
  std::vector<Filter> res;

  auto sig_1h = m.get_signal(H_1);
  auto sig_4h = m.get_signal(H_4);
  auto sig_1d = m.get_signal(D_1);

  // Higher timeframe alignment (from confirmations)
  if (sig_1h.type == Rating::Entry) {
    if (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist)
      res.emplace_back(Trend::ModerateUptrend, Confidence::High, "4h confirms",
                       "4H timeframe confirms 1H entry signal");

    if (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist)
      res.emplace_back(Trend::StrongUptrend, Confidence::High, "1d confirms",
                       "Daily timeframe confirms 1H entry signal");
  }

  // Daily trend alignment (from confirmations)
  const auto& ind_1d = m.ind_1d;
  bool daily_uptrend = ind_1d.ema21(-1) > ind_1d.ema50(-1);
  bool price_above_daily_ema = ind_1d.price(-1) > ind_1d.ema21(-1);

  if (daily_uptrend && price_above_daily_ema)
    res.emplace_back(Trend::ModerateUptrend, Confidence::High,
                     tagged("1d aligned", GREEN),
                     "Daily trend alignment: EMA21 > EMA50, price above EMA21");

  return res;
}
