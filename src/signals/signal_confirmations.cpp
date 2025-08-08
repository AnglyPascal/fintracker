#include "config.h"
#include "indicators.h"
#include "signals.h"

inline auto& sig_config = config.sig_config;

// Entry confirmations

Confirmation higher_timeframe_alignment_confirmation(const Metrics& metrics) {
  auto sig_1h = metrics.get_signal(H_1, -1);
  auto sig_4h = metrics.get_signal(H_4, -1);
  auto sig_1d = metrics.get_signal(D_1, -1);

  if (sig_1h.type == Rating::Entry) {
    if (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist)
      return "4h entry";

    if (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist)
      return "1d entry";
  }

  return "";
}

inline constexpr conf_f confirmation_funcs[] = {
    higher_timeframe_alignment_confirmation,
};

std::vector<Confirmation> confirmations(const Metrics& m) {
  std::vector<Confirmation> res;
  for (auto f : confirmation_funcs)
    res.push_back(f(m));
  return res;
}
