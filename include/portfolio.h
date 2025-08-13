#pragma once

#include "api.h"
#include "calendar.h"
#include "config.h"
#include "indicators.h"
#include "position_sizing.h"
#include "positions.h"
#include "replay.h"
#include "signals.h"
#include "symbols.h"

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

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

using Tickers = std::map<std::string, Ticker>;

enum class FormatTarget;

inline constexpr int max_concurrency = 32;

class Portfolio {
 private:
  Symbols symbols;
  mutable std::shared_mutex mtx;

 public:
  TG tg;

 private:
  TD td;
  Replay rp;

  Tickers tickers;
  OpenPositions positions;
  Calendar calendar;

 public:
  LocalTimePoint last_updated;

 private:
  const minutes update_interval;

 public:
  Portfolio() noexcept;
  void run();
  void update_trades();

  std::pair<const Position*, double> add_trade(const Trade& trade) const;

 private:
  void run_replay();

  template <typename... Args>
  auto time_series(Args&&... args) {
    return rp.enabled ? rp.time_series(std::forward<Args>(args)...)
                      : td.time_series(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto real_time(Args&&... args) {
    return rp.enabled ? rp.real_time(std::forward<Args>(args)...)
                      : td.real_time(std::forward<Args>(args)...);
  }

  void add_candle();
  void rollback();

  void add_candle_sync();

 public:  // getters
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

  void write_history() const;
  void write_tickers() const;
  void write_index() const;

  void write_page() const {
    write_index();
    write_tickers();
    write_history();
  }
};
