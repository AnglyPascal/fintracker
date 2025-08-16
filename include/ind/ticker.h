#pragma once

#include "calendar.h"
#include "indicators.h"

#include "util/times.h"
#include "signals/position_sizing.h"
#include "signals/signals.h"

#include <string>

struct Ticker {
  const std::string symbol;
  const int priority;

  TimePoint last_polled;

  Metrics metrics;
  CombinedSignal signal;

  StopLoss stop_loss;
  PositionSizing position_sizing;

  Event ev;

 private:
  friend class Portfolio;

  Ticker(const std::string& symbol,
         int priority,
         std::vector<Candle>&& candles,
         minutes update_interval,
         const Position* position,
         const Event& ev) noexcept
      : symbol{symbol},
        priority{priority},
        last_polled{Clock::now()},
        metrics{std::move(candles), update_interval, position},
        ev{ev}  //
  {
    calculate_signal();
  }

  void write_plot_data() const;
  CombinedSignal gen_signal(int idx = -1) const;

  void update_position(const Position* pos) {
    metrics.update_position(pos);
    calculate_signal();
  }

  void push_back(const Candle& next, const Position* pos) {
    metrics.push_back(next, pos);
    calculate_signal();
  }

  void rollback() {
    metrics.rollback();
    calculate_signal();
  }

  void calculate_signal();
};
