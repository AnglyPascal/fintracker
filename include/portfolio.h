#pragma once

#include "api.h"
#include "backtest.h"
#include "calendar.h"
#include "config.h"
#include "indicators.h"
#include "positions.h"
#include "signal.h"
#include "times.h"

#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

struct Ticker {
  const std::string symbol;
  int priority;  // 1 = high, 2 = medium, 3 = low

  TimePoint last_polled;

  Metrics metrics;
  Signal signal = {};
  std::string long_term_trend;

  Ticker(const std::string& symbol,
         int priority,
         std::vector<Candle>&& candles,
         minutes update_interval,
         const Position* position,
         const std::string& long_term_trend) noexcept;

  void plot() const;
};

using Tickers = std::map<std::string, Ticker>;

struct SymbolInfo {
  std::string symbol;
  int priority;
};

enum class FormatTarget;

class Portfolio {
 private:
  Config config;
  std::vector<SymbolInfo> symbols;

 public:
  TG tg;

 private:
  TD td;
  Backtest bt;

  Tickers tickers;
  OpenPositions positions;
  Calendar calendar;

  const minutes update_interval;

  mutable std::shared_mutex mtx;

 public:
  Portfolio(Config config) noexcept;
  void run();

  template <typename... Args>
  auto reader_lock(Args&&... args) const {
    return std::shared_lock(mtx, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto writer_lock(Args&&... args) {
    return std::unique_lock(mtx, std::forward<Args>(args)...);
  }

  std::pair<const Position*, double> add_trade(const Trade& trade) const;

  LocalTimePoint last_updated() const;

 private:
  void run_backtest();

  auto time_series(const std::string& symbol) {
    return bt.enabled ? bt.time_series(symbol) : td.time_series(symbol);
  }

  auto real_time(const std::string& symbol) {
    return bt.enabled ? bt.real_time(symbol) : td.real_time(symbol);
  }

  void add_candle();
  void rollback();

  void plot(const std::string& symbol = "") const;
  void write_page(const std::string& symbol = "") const;

  static std::vector<SymbolInfo> read_symbols();

  friend class Notifier;

  template <FormatTarget target, typename T>
  friend std::string to_str(const T& t);

  template <FormatTarget target, typename T, typename S>
  friend std::string to_str(const T& t, const S& s);

 public:  // getters
  const Trades& get_trades() const { return positions.get_trades(); }
  const Positions& get_positions() const { return positions.get_positions(); }

  const Ticker* get_ticker(const std::string& symbol) const {
    auto it = tickers.find(symbol);
    return it == tickers.end() ? nullptr : &it->second;
  }

  bool is_tracking(const std::string& symbol) const {
    return tickers.find(symbol) != tickers.end();
  }
};

