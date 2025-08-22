#include "ind/indicators.h"
#include "sig/signals.h"
#include "ind/stop_loss.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

// Stop Loss tests
StopHit stop_loss_hits(const Metrics& m, const StopLoss& stop_loss) {
  if (!m.has_position())
    return StopHitType::None;

  auto days_held =
      std::chrono::floor<days>(std::chrono::floor<days>(now_ny_time()) -
                               std::chrono::floor<days>(m.position->tp));
  if (days_held.count() > sig_config.stop_max_holding_days)
    return StopHitType::TimeExit;

  auto price = m.last_price();
  if (price < stop_loss.final_stop)
    return StopHitType::StopLossHit;

  auto dist = price - stop_loss.final_stop;

  if (dist > 0 && dist < m.ind_1h.atr(-1) * sig_config.stop_atr_proximity)
    return StopHitType::StopProximity;

  // FIXME what's this?
  if (dist < 1.0 * stop_loss.atr_stop - stop_loss.final_stop)
    return StopHitType::StopInATR;

  return StopHitType::None;
}
