#pragma once

#include "calendar.h"
#include "indicators.h"
#include "risk/risk.h"
#include "sig/signals.h"
#include "util/symbols.h"

#include <iostream>

struct Ticker {
  const SymbolInfo si;
  Event ev;

  const Indicators& spy;
  const OpenPositions& open_positions;

  Metrics metrics;
  CombinedSignal signal;
  Risk risk;

 private:
  friend class Portfolio;

  void calc_signal() {
    signal = CombinedSignal{metrics, ev};
    risk = Risk{metrics, spy, LocalTimePoint{}, signal, open_positions, ev};
    signal.apply_stop_hit(risk.get_stop_hit(metrics));
  }

  template <typename... Args>
  Ticker(const SymbolInfo& si,
         const Event& ev,
         const Indicators& spy,
         const OpenPositions& open_positions,
         Args&&... args) noexcept
      : si{si},
        ev{ev},
        spy{spy},
        open_positions{open_positions},
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
