#include "sig/signals.h"
#include "util/config.h"

#include <spdlog/spdlog.h>

inline auto& sig_config = config.sig_config;

SignalMemory::SignalMemory(minutes interval) noexcept {
  if (interval == H_1)
    memory_length = 16;
  else if (interval == H_4)
    memory_length = 10;
  else
    memory_length = 6;
}

double SignalMemory::score() const {
  double scr = 0.0;
  double weight = 0.0;

  double lambda = sig_config.score_memory_lambda;
  for (auto& sig : past) {
    scr = scr * lambda + sig.score;
    weight = weight * lambda + lambda;
  }

  return weight > 0.0 ? scr / weight : 0.0;
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
