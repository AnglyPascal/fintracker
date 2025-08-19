#pragma once

#include "calendar.h"
#include "indicators.h"
#include "sig/position_sizing.h"
#include "sig/signals.h"
#include "util/symbols.h"

struct Ticker {
  const SymbolInfo si;
  Event ev;

  Metrics metrics;
  StopLoss stop_loss;

  CombinedSignal signal;
  PositionSizing position_sizing;

 private:
  friend class Portfolio;

  void calc_signal() {
    stop_loss = StopLoss{metrics};
    signal = CombinedSignal{metrics, stop_loss, ev};
    position_sizing = PositionSizing{metrics, signal, stop_loss};
  }

  template <typename... Args>
  Ticker(const SymbolInfo& si, const Event& ev, Args&&... args) noexcept
      : si{si},
        ev{ev},
        metrics{std::forward<Args>(args)...}  //
  {
    calc_signal();
  }

  template <typename... Args>
  void update_position(Args&&... args) {
    metrics.update_position(std::forward<Args>(args)...);
    calc_signal();
  }

  template <typename... Args>
  void push_back(Args&&... args) {
    metrics.push_back(std::forward<Args>(args)...);
    calc_signal();
  }

  template <typename... Args>
  void rollback(Args&&... args) {
    metrics.rollback(std::forward<Args>(args)...);
    calc_signal();
  }

  void write_plot_data() const;
};
