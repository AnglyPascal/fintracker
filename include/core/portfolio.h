#pragma once

#include "positions.h"
#include "replay.h"

#include "ind/ticker.h"
#include "mt/api.h"
#include "mt/message.h"
#include "util/config.h"
#include "util/symbols.h"
#include "util/times.h"

#include <shared_mutex>
#include <string>
#include <thread>

using Tickers = std::map<std::string, Ticker>;
enum class FormatTarget;

class Portfolio : public Endpoint {
 private:
  Symbols symbols;

  mutable std::shared_mutex mtx;
  std::thread server;

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
  void run();

 private:
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

  void run_replay();

  std::pair<const Position*, double> add_trade(const Trade& trade);
  void update_trades();

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

  LocalTimePoint last_candle_time() const {
    if (tickers.empty())
      return {};
    return tickers.begin()->second.metrics.last_updated();
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
  void iter();
};
