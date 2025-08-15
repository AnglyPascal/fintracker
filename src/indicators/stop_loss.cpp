#include "core/positions.h"
#include "ind/indicators.h"
#include "util/config.h"

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

  auto support_1d = m.ind_1d.nearest_support(-1);
  auto support_4h = m.ind_4h.nearest_support(-1);
  auto support_1h = m.ind_1h.nearest_support(-1);

  auto primary_support =
      support_1d ? support_1d : (support_4h ? support_4h : support_1h);

  swing_low = 0.0;
  if (primary_support) {
    double buffer = support_1d ? 0.012 : support_4h ? 0.008 : 0.005;
    swing_low = primary_support->get().lo * (1 - buffer);
  }

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
