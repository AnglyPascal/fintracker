#pragma once

#include "api.h"
#include "calendar.h"
#include "config.h"
#include "indicators.h"
#include "position_sizing.h"
#include "positions.h"
#include "replay.h"
#include "signals.h"

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

struct Ticker {
  const std::string symbol;
  int priority;  // 1 = high, 2 = medium, 3 = low

  TimePoint last_polled;

  Metrics metrics;
  CombinedSignal signal;

  StopLoss stop_loss;
  const PositionSizingConfig& sizing_config;
  PositionSizing position_sizing;

  Ticker(const std::string& symbol,
         int priority,
         std::vector<Candle>&& candles,
         minutes update_interval,
         const Position* position,
         const PositionSizingConfig& config) noexcept;

  void write_plot_data() const;
  CombinedSignal gen_signal(int idx = -1) const;

  void calculate_signal() {
    stop_loss = StopLoss(metrics, sizing_config);
    signal = gen_signal(-1);
    position_sizing =
        PositionSizing(metrics, signal, stop_loss, sizing_config);
  }
};

using Tickers = std::map<std::string, Ticker>;

struct SymbolInfo {
  std::string symbol;
  int priority;
};

enum class FormatTarget;

inline constexpr int max_concurrency = 32;

class Portfolio {
 public:
  const Config config;
  const PositionSizingConfig sizing_config;

 private:
  std::vector<SymbolInfo> symbols;
  mutable std::shared_mutex mtx;

 public:
  TG tg;

 private:
  TD td;
  Replay rp;

  Tickers tickers;
  OpenPositions positions;
  Calendar calendar;

  LocalTimePoint _last_updated;

  const minutes update_interval;

 public:
  Portfolio(Config config, PositionSizingConfig sizing_config) noexcept;
  void run();
  void update_trades();

  std::pair<const Position*, double> add_trade(const Trade& trade) const;

 private:
  void run_replay();

  auto time_series(auto& symbol) {
    return rp.enabled ? rp.time_series(symbol) : td.time_series(symbol);
  }

  auto real_time(auto& symbol) {
    return rp.enabled ? rp.real_time(symbol) : td.real_time(symbol);
  }

  void add_candle();
  void rollback();

  void add_candle_sync();

 public:  // getters
  LocalTimePoint last_updated() const;

  auto& get_trades() const { return positions.get_trades(); }
  auto& get_positions() const { return positions.get_positions(); }

  const Ticker* get_ticker(const std::string& symbol) const {
    auto it = tickers.find(symbol);
    return it == tickers.end() ? nullptr : &it->second;
  }

  bool is_tracking(const std::string& symbol) const {
    return tickers.find(symbol) != tickers.end();
  }

  template <typename... Args>
  auto reader_lock(Args&&... args) const {
    return std::shared_lock(mtx, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto writer_lock(Args&&... args) {
    return std::unique_lock(mtx, std::forward<Args>(args)...);
  }

 private:
  friend class Notifier;

  template <FormatTarget target, typename T>
  friend std::string to_str(const T& t);

  template <FormatTarget target, typename T, typename S>
  friend std::string to_str(const T& t, const S& s);

  void write_plot_data(const std::string& symbol) const;
  void plot(const std::string& symbol = "") const;

  void write_page() const;
};

