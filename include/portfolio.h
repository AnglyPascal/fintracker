#pragma once

#include "api.h"
#include "indicators.h"
#include "positions.h"
#include "signal.h"

#include <chrono>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct Ticker {
  const std::string symbol;
  int priority;  // 1 = high, 2 = medium, 3 = low

  TimePoint last_polled;

  Metrics metrics;
  Signal signal = {};

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

enum class FormatTarget;

class Portfolio {
  std::vector<SymbolInfo> symbols;
  TD td;

  Tickers tickers;
  OpenPositions positions;

  TimePoint last_updated;
  const minutes update_interval;

  mutable std::shared_mutex mtx;

 public:
  Portfolio() noexcept;
  void run();

  template <typename... Args>
  auto reader_lock(Args&&... args) const {
    return std::shared_lock(mtx, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto writer_lock(Args&&... args) {
    return std::unique_lock(mtx, std::forward<Args>(args)...);
  }

  void add_trade(const Trade& trade) const;

 private:
  void add_candle();

  void plot(const std::string& ticker) const;
  void write_page() const;

  static std::vector<SymbolInfo> read_symbols();

  friend class Notifier;

  template <FormatTarget target, typename T>
  friend std::string to_str(const T& t);

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

