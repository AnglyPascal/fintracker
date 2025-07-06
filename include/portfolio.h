#pragma once

#include "api.h"
#include "indicators.h"
#include "positions.h"
#include "prediction.h"
#include "signal.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct Ticker {
  const std::string symbol;
  int priority;  // 1 = high, 2 = low

  TimePoint last_polled;

  Metrics metrics;
  Signal signal = {};

  const Position* position = nullptr;

  Ticker(const std::string& symbol,
         int priority,
         std::vector<Candle>&& candles,
         minutes update_interval,
         const Position* position) noexcept;

  std::string to_str(bool tg = false) const;
  void plot() const;

 private:
  std::string pos_to_str(bool tg = false) const;
};

using Tickers = std::map<std::string, Ticker>;
using SymbolInfo = std::pair<std::string, int>;

class Portfolio {
  TD td;

  Tickers tickers;
  OpenPositions positions;

  TimePoint last_updated;
  minutes update_interval;

  mutable int count = 0;
  mutable bool send_signal_alerts = false;

 private:
  int get_priority(const Ticker& ticker) const;
  void update();

 public:
  void send_current_positions(const std::string& ticker) const {
    positions.send_current_positions(ticker);
  }
  void status(const std::string& ticker) const;

 private:
  void send_tg_update(bool full = false) const;
  void send_tg_alert() const;
  void console_update() const;

  void send_updates() const;

 public:
  Portfolio(const std::vector<SymbolInfo>& symbols) noexcept;

  void debug() const;
  void run();
  void plot(const std::string& ticker) const;
};
