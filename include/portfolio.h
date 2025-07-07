#pragma once

#include "api.h"
#include "format.h"
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

  void plot() const;

  bool has_position() const;
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

 private:
  int get_priority(const Ticker& ticker) const;
  void update();

 public:
  void send_current_positions(const std::string& ticker) const;
  void status(const std::string& ticker) const;
  const Trades& get_trades() const { return positions.get_trades(); }

 private:
  mutable int last_tg_update_msg_id = -1;
  void send_tg_update() const;
  void send_tg_alert() const;

  void send_updates() const;
  void write_html() const;

 public:
  Portfolio(const std::vector<SymbolInfo>& symbols) noexcept;

  void debug() const;
  void run();

  void plot(const std::string& ticker) const;
  void plot_all() const;


  template <FormatTarget target, typename T>
  friend std::string to_str(const T& t);
};
