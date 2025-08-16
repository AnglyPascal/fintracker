#include "ind/ticker.h"

#include <spdlog/spdlog.h>

void Ticker::calculate_signal() {
  stop_loss = StopLoss(metrics);
  signal = gen_signal(-1);
  position_sizing = PositionSizing(metrics, signal, stop_loss);

  spdlog::trace("[stop] {}: {} price={:.2f} atr_stop={:.2f} final={:.2f}",
                symbol, stop_loss.is_trailing, metrics.last_price(),
                stop_loss.atr_stop, stop_loss.final_stop);
}
