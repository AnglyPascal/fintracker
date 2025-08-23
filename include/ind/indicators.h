#pragma once

#include "backtest.h"
#include "candle.h"
#include "support_resistance.h"
#include "trendlines.h"

#include "core/positions.h"
#include "sig/signals.h"
#include "util/times.h"

#include <cassert>
#include <deque>
#include <map>
#include <string>
#include <vector>

struct EMA {
  std::vector<double> values;

 private:
  int period;

 public:
  EMA() noexcept = default;
  EMA(const std::vector<Candle>& candles, int period) noexcept;
  EMA(const std::vector<double>& prices, int period) noexcept;

  void push_back(const Candle& candle) noexcept;
  void push_back(double price) noexcept;
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

  void push_back(const Candle& candle) noexcept;
  void pop_back() noexcept { values.pop_back(); }

  bool rising() const;
};

struct MACD {
  std::vector<double> macd_line;
  EMA signal_ema;
  std::vector<double> histogram;

 private:
  EMA fast_ema;
  EMA slow_ema;

 public:
  MACD(const std::vector<Candle>& candles,
       int fast = 12,
       int slow = 26,
       int signal = 9) noexcept;

  void push_back(const Candle& candle) noexcept;
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

  void push_back(const Candle& candle) noexcept;
  void pop_back(double close) noexcept {
    prev_close = close;
    values.pop_back();
  }
};

struct Pullback {
  double recent_high;
  double pb;
};

struct IndicatorsCore {
 public:
  minutes interval;

 protected:
  std::vector<Candle> candles;

  EMA _ema9, _ema21, _ema50;
  RSI _rsi;
  MACD _macd;
  ATR _atr;

  IndicatorsCore(std::vector<Candle>&& c, minutes inv) noexcept
      : interval{inv},
        candles{std::move(c)},
        _ema9{candles, 9},
        _ema21{candles, 21},
        _ema50{candles, 50},
        _rsi{candles},
        _macd{candles},
        _atr{candles}  //
  {}

  size_t sanitize(int idx) const {
    return idx < 0 ? candles.size() + idx : idx;
  }

 public:
  auto size() const { return candles.size(); }
  LocalTimePoint time(int idx) const { return candles[sanitize(idx)].time(); }

  double price(int idx) const { return candles[sanitize(idx)].price(); }
  double low(int idx) const { return candles[sanitize(idx)].low; }
  double close(int idx) const { return candles[sanitize(idx)].close; }
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

  size_t candles_per_day() const {
    minutes day{D_1 + interval - minutes{1}};
    return day / interval;
  }
};

struct IndicatorsTrends : public IndicatorsCore {
 protected:
  Trends trends;
  Support support;
  Resistance resistance;

  IndicatorsTrends(std::vector<Candle>&& c, minutes inv) noexcept
      : IndicatorsCore{std::move(c), inv},
        trends{*this},
        support{*this},
        resistance{*this}  //
  {}

 public:
  TrendLine price_trend(int idx) const {
    return idx == -1 ? trends.price[0] : Trends::price_trends(*this, idx)[0];
  }

  TrendLine rsi_trend(int idx) const {
    return idx == -1 ? trends.rsi[0] : Trends::rsi_trends(*this, idx)[0];
  }

  TrendLine ema21_trend(int idx) const {
    return idx == -1 ? trends.ema21[0] : Trends::ema21_trends(*this, idx)[0];
  }

  auto& support_zones() const { return support.zones; }
  auto& resistance_zones() const { return resistance.zones; }

  auto nearest_support_below(int idx) const {
    return support.nearest_below(price(idx));
  }
  auto nearest_support_above(int idx) const {
    return support.nearest_above(price(idx));
  }

  auto nearest_resistance_below(int idx) const {
    return resistance.nearest_below(price(idx));
  }
  auto nearest_resistance_above(int idx) const {
    return resistance.nearest_above(price(idx));
  }
};

struct Stats {
  std::map<ReasonType, SignalStats> reason;
  std::map<HintType, SignalStats> hint;

  Stats() = default;
  Stats(const IndicatorsTrends& ind) : Stats{Backtest{ind}} {}

 private:
  static std::map<ReasonType, SignalStats> get_reason_stats(const Backtest& bt);
  static std::map<HintType, SignalStats> get_hint_stats(const Backtest& bt);

  Stats(Backtest&& bt)
      : reason{get_reason_stats(bt)}, hint{get_hint_stats(bt)} {}
};

struct Indicators : public IndicatorsTrends {
 public:
  Signal signal;
  Stats stats;

  friend struct Metrics;

 public:
  Indicators(std::vector<Candle>&& candles, minutes interval) noexcept
      : IndicatorsTrends{std::move(candles), interval},
        stats{*this}  //
  {
    signal = Signal{*this};
  }

  Indicators(const Indicators&) = delete;
  Indicators& operator=(const Indicators&) = delete;

  Indicators(Indicators&&) = default;
  Indicators& operator=(Indicators&&) = default;

  void push_back(const Candle& candle) noexcept;
  void pop_back() noexcept;

  LocalTimePoint plot(const std::string& sym, const std::string& time) const;

  Signal get_signal(int idx) const;
};

struct Metrics {
  std::vector<Candle> candles;
  minutes interval;

  Indicators ind_1h, ind_4h, ind_1d;
  const Position* position;

 public:
  Metrics(std::vector<Candle>&& candles,
          minutes interval,
          const Position* position) noexcept;

  Metrics(const Metrics&) = delete;
  Metrics& operator=(const Metrics&) = delete;

  Metrics(Metrics&&) = default;
  Metrics& operator=(Metrics&&) = default;

  bool push_back(const Candle& next, const Position* position) noexcept;
  void rollback() noexcept;

  auto last_price() const { return ind_1h.price(-1); }
  auto last_updated() const { return ind_1h.time(-1); }

  const Indicators& get_indicators(minutes interval) const {
    return interval == H_1 ? ind_1h : (interval == H_4 ? ind_4h : ind_1d);
  }

  Signal get_signal(minutes interval, int idx = -1) const {
    return get_indicators(interval).get_signal(idx);
  }

  auto& get_stats(minutes interval) const {
    return get_indicators(interval).stats;
  }

  Forecast gen_forecast(int idx) const;

  bool has_position() const;
  void update_position(const Position* pos);

  LocalTimePoint plot(const std::string& sym) const;
};
