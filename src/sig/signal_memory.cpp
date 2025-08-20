#include "sig/signals.h"
#include "util/config.h"

#include <spdlog/spdlog.h>

inline auto& sig_config = config.sig_config;

SignalMemory::SignalMemory(minutes inv) noexcept {
  memory_length = inv == H_1 ? 16 : inv == H_4 ? 10 : 6;
}

Score SignalMemory::score() const {
  double entry = 0.0, exit = 0.0, final = 0.0;
  double weight = 0.0;

  double lambda = sig_config.score_memory_lambda;
  for (auto& sig : past) {
    entry = entry * lambda + sig.score.entry;
    exit = exit * lambda + sig.score.exit;
    final = final * lambda + sig.score.final;
    weight = weight * lambda + 1;
  }

  if (weight == 0.0)
    return {};

  return {entry / weight, exit / weight, 0.0, final / weight};
}

int SignalMemory::rating_score() const {
  int n_entry = 0;
  int n_strong_watchlist = 0;
  int n_exit = 0;
  int n_strong_caution = 0;

  auto strength_threshold = sig_config.mem_strength_threshold;
  for (auto& sig : past) {
    switch (sig.type) {
      case Rating::Entry:
        n_entry += 1 + (sig.score >= strength_threshold);
        break;
      case Rating::Watchlist:
        n_strong_watchlist += sig.score >= strength_threshold;
        break;
      case Rating::Exit:
        n_exit += 1 + (sig.score <= -strength_threshold);
        break;
      case Rating::Caution:
      case Rating::HoldCautiously:
        n_strong_caution += sig.score <= -strength_threshold;
        break;
      default:
        break;
    }
  }

  n_entry += (n_strong_watchlist + 1) / 2;
  n_exit += (n_strong_caution + 1) / 2;

  return n_entry - n_exit;
}
