#include "config.h"
#include "indicators.h"
#include "positions.h"

auto& sizing_config = config.sizing_config;

StopLoss::StopLoss(const Metrics& m) noexcept {
  auto& ind = m.ind_1h;

  auto price = ind.price(-1);
  auto atr = ind.atr(-1);

  auto pos = m.position;
  auto entry_price = m.has_position() ? pos->px : price;
  auto max_price_seen = m.has_position() ? pos->max_price_seen : price;

  // trailing kicks in after ~1R move
  is_trailing =
      m.has_position() &&
      (max_price_seen > entry_price + sizing_config.trailing_trigger_atr * atr);

  auto n = std::min((size_t)sizing_config.swing_low_window, ind.size());

  swing_low = std::numeric_limits<double>::max();
  for (int i = -n; i <= -1; ++i)
    swing_low = std::min(swing_low, ind.low(i));
  swing_low *= 1.0 - sizing_config.swing_low_buffer;

  double hard_stop = 0.0;

  // Initial Stop
  if (!is_trailing) {
    ema_stop = ind.ema21(-1) * (1.0 - sizing_config.ema_stop_pct);
    auto best = std::min(swing_low, ema_stop) * 0.999;

    atr_stop = entry_price - sizing_config.stop_atr_multiplier * atr;
    hard_stop = entry_price * (1.0 - sizing_config.stop_pct);

    final_stop = std::max({best, atr_stop, hard_stop});
    stop_pct = (entry_price - final_stop) / entry_price;
  }

  // Trailing Stop
  else {
    atr_stop = max_price_seen - sizing_config.trailing_atr_multiplier * atr;
    hard_stop = max_price_seen * (1.0 - sizing_config.trailing_stop_pct);

    final_stop = std::max({swing_low, atr_stop, hard_stop});
    stop_pct = (price - final_stop) / price;
  }

}
