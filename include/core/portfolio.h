#pragma once

#include "concurrency/api.h"
#include "concurrency/message.h"
#include "core/calendar.h"
#include "core/positions.h"
#include "core/replay.h"
#include "ind/indicators.h"
#include "signals/position_sizing.h"
#include "signals/signals.h"
#include "util/config.h"
#include "util/symbols.h"
#include "util/times.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
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
using sleep_f = std::function<bool(std::chrono::milliseconds)>;

inline constexpr int max_concurrency = 32;

class Portfolio : public Endpoint {
 private:
  Symbols symbols;
  mutable std::shared_mutex mtx;

  TD td;
  Replay rp;

  Tickers tickers;
  OpenPositions positions;
  Calendar calendar;

  const minutes update_interval;
  std::string tunnel_url;

 public:
  LocalTimePoint last_updated;

  Portfolio() noexcept;
  ~Portfolio() noexcept;

  void run(sleep_f sleep);
  void update_trades();

  std::pair<const Position*, double> add_trade(const Trade& trade) const;

 private:
  void run_replay(sleep_f sleep);

  template <typename... Args>
  auto time_series(Args&&... args) {
    return config.replay_en ? rp.time_series(std::forward<Args>(args)...)
                            : td.time_series(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto real_time(Args&&... args) {
    return config.replay_en ? rp.real_time(std::forward<Args>(args)...)
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

 private:
  void handle_command(const Message& msg);
  std::thread server;
  void iter();
};
