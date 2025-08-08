#pragma once

#include "backtest.h"
#include "candle.h"
#include "positions.h"
#include "signals.h"
#include "support_resistance.h"
#include "times.h"
#include "trendlines.h"

#include <cassert>
#include <deque>
#include <string>
#include <vector>

std::vector<Candle> downsample(std::vector<Candle>& candles,
                               minutes source,
                               minutes target);

Candle latest_candle(std::vector<Candle>& candles,
                     minutes source,
                     minutes target);

struct EMA {
  std::vector<double> values;

 private:
  int period;

 public:
  EMA() noexcept = default;
  EMA(const std::vector<Candle>& candles, int period) noexcept;
  EMA(const std::vector<double>& prices, int period) noexcept;

  void add(const Candle& candle) noexcept;
  void add(double price) noexcept;
  void pop_back() noexcept { values.pop_back(); }
};

struct RSI {
  std::vector<double> values;

 private:
  int period;

  std::deque<double> gains;
  std::deque<double> losses;
  double last_price = 0.0;
  double avg_gain = 0.0;
  double avg_loss = 0.0;

 public:
  RSI(const std::vector<Candle>& candles, int period = 14) noexcept;

  void add(const Candle& candle) noexcept;
  void pop_back() noexcept { values.pop_back(); }

  bool rising() const;
};

struct MACD {
  std::vector<double> macd_line;
  EMA signal_ema;
  std::vector<double> histogram;

 private:
  int fast_period = 12;
  int slow_period = 26;
  int signal_period = 9;

  EMA fast_ema;
  EMA slow_ema;

 public:
  MACD(const std::vector<Candle>& candles,
       int fast = 12,
       int slow = 26,
       int signal = 9) noexcept;

  void add(const Candle& candle) noexcept;
  void pop_back() noexcept;
};

struct ATR {
  std::vector<double> values;

 private:
  int period = 14;
  double prev_close = 0.0;

 public:
  ATR() noexcept = default;
  ATR(const std::vector<Candle>& candles, int period = 14) noexcept;

  void add(const Candle& candle) noexcept;
  void pop_back(double close) noexcept {
    prev_close = close;
    values.pop_back();
  }
};

struct Pullback {
  double recent_high;
  double pb;
};

struct Indicators {
  const minutes interval;

 private:
  std::vector<Candle> candles;

  EMA _ema9, _ema21, _ema50;
  RSI _rsi;
  MACD _macd;
  ATR _atr;

  Trends trends;

  SupportResistance<SR::Support> support;
  SupportResistance<SR::Resistance> resistance;

 public:
  Signal signal;
  SignalMemory memory;

  void get_stats();
  Stats stats;

  friend struct Metrics;

 public:
  Indicators(std::vector<Candle>&& candles, minutes interval) noexcept;

  void add(const Candle& candle) noexcept;
  void pop_back() noexcept;
  void pop_memory() noexcept;

  LocalTimePoint plot(const std::string& sym, const std::string& time) const;

  Signal gen_signal(int idx) const;
  Forecast gen_forecast(int idx) const;

  size_t sanitize(int idx) const {
    return idx < 0 ? candles.size() + idx : idx;
  }

  auto size() const { return candles.size(); }
  LocalTimePoint time(int idx) const { return candles[sanitize(idx)].time(); }

  double price(int idx) const { return candles[sanitize(idx)].price(); }
  double low(int idx) const { return candles[sanitize(idx)].low; }
  double high(int idx) const { return candles[sanitize(idx)].high; }
  int volume(int idx) const { return candles[sanitize(idx)].volume; }

  double ema9(int idx) const { return _ema9.values[sanitize(idx)]; }
  double ema21(int idx) const { return _ema21.values[sanitize(idx)]; }
  double ema50(int idx) const { return _ema50.values[sanitize(idx)]; }

  double atr(int idx) const { return _atr.values[sanitize(idx)]; }
  double rsi(int idx) const { return _rsi.values[sanitize(idx)]; }

  double macd(int idx) const { return _macd.macd_line[sanitize(idx)]; }
  double macd_signal(int idx) const {
    return _macd.signal_ema.values[sanitize(idx)];
  }
  double hist(int idx) const { return macd(idx) - macd_signal(idx); }

  TrendLine price_trend(int idx) const {
    return idx == -1 ? trends.price[0] : Trends::price_trends(*this, idx)[0];
  }

  TrendLine rsi_trend(int idx) const {
    return idx == -1 ? trends.rsi[0] : Trends::rsi_trends(*this, idx)[0];
  }

  TrendLine ema21_trend(int idx) const {
    return idx == -1 ? trends.ema21[0] : Trends::ema21_trends(*this, idx)[0];
  }

  Pullback pullback(size_t lookback = 360) const {
    if (candles.size() < lookback)
      lookback = candles.size();

    double high = 0.0;
    for (size_t i = candles.size() - lookback; i < candles.size(); ++i)
      high = std::max(high, candles[i].high);

    return {high, (high - price(-1)) / high * 100.0};
  }

  auto& support_zones() const { return support.zones; }
  auto& resistance_zones() const { return resistance.zones; }
};

struct Metrics {
  std::vector<Candle> candles;
  const minutes interval;

  Indicators ind_1h, ind_4h, ind_1d;
  const Position* position;

 public:
  Metrics(std::vector<Candle>&& candles,
          minutes interval,
          const Position* position) noexcept;

  bool add(const Candle& candle, const Position* position) noexcept;
  Candle rollback() noexcept;

  auto last_price() const { return candles.back().price(); }
  auto last_updated() const {
    return datetime_to_local(candles.back().datetime);
  }
  Signal get_signal(minutes interval, int idx) const;
  bool has_position() const;
  void update_position(const Position* pos);

  LocalTimePoint plot(const std::string& sym) const;
};

struct PositionSizingConfig;

struct StopLoss {
  double swing_low = 0.0;
  double ema_stop = 0.0;
  double atr_stop = 0.0;
  double final_stop = 0.0;
  double stop_pct = 0.0;
  bool is_trailing = false;

  StopLoss() noexcept = default;
  StopLoss(const Metrics& m, const PositionSizingConfig& config) noexcept;
};
