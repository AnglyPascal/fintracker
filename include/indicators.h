#pragma once

#include "prediction.h"
#include "times.h"

#include <cassert>
#include <deque>
#include <string>
#include <vector>

struct Candle {
  std::string datetime = "";
  double open = 0.0;
  double high = 0.0;
  double low = 0.0;
  double close = 0.0;
  int volume = 0.0;

  std::string day() const { return datetime.substr(0, 10); }
  double price() const { return close; }
  LocalTimePoint time() const { return datetime_to_local(datetime); }
};

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

struct Indicators {
  std::vector<Candle> candles;
  const minutes interval;

  EMA ema9, ema21, ema50;
  RSI rsi;
  MACD macd;
  ATR atr;

  Trends trends;

  Indicators(std::vector<Candle>&& candles, minutes interval) noexcept;

  void add(const Candle& candle) noexcept;
  void pop_back() noexcept;

  void plot(const std::string& sym) const;
};

struct Position;

struct StopLoss {
  double swing_low = 0.0;
  double ema_stop = 0.0;
  double atr_stop = 0.0;
  double final_stop = 0.0;
  double stop_pct = 0.0;
  bool is_trailing = false;

  StopLoss() noexcept = default;
  StopLoss(const Indicators& ind, const Position* pos) noexcept;
};

struct Pullback {
  double recent_high;
  double pb;
};

std::vector<Candle> downsample(const std::vector<Candle>& candles,
                               minutes source,
                               minutes target);

struct Metrics {
  std::vector<Candle> candles;
  const minutes interval;

  Indicators ind_1h;
  StopLoss stop_loss;
  const Position* position;

 public:
  Metrics(std::vector<Candle>&& candles,
          minutes interval,
          const Position* position) noexcept;

  bool add(const Candle& candle, const Position* position) noexcept;
  Candle pop_back() noexcept;

  auto last_price() const { return candles.back().price(); }
  auto last_updated() const {
    return datetime_to_local(candles.back().datetime);
  }

  Pullback pullback(size_t lookback = 360) const;
  bool has_position() const;

  void plot(const std::string& sym) const;

 private:
  size_t sanitize(int idx) const {
    return idx < 0 ? ind_1h.candles.size() + idx : idx;
  }

 public:
  double price(int idx) const { return ind_1h.candles[sanitize(idx)].price(); }

  double ema9(int idx) const { return ind_1h.ema9.values[sanitize(idx)]; }
  double ema21(int idx) const { return ind_1h.ema21.values[sanitize(idx)]; }
  double ema50(int idx) const { return ind_1h.ema50.values[sanitize(idx)]; }

  double atr(int idx) const { return ind_1h.atr.values[sanitize(idx)]; }
  double rsi(int idx) const { return ind_1h.rsi.values[sanitize(idx)]; }

  double macd(int idx) const { return ind_1h.macd.macd_line[sanitize(idx)]; }
  double signal(int idx) const {
    return ind_1h.macd.signal_ema.values[sanitize(idx)];
  }
  double hist(int idx) const { return macd(idx) - signal(idx); }

  TrendLine price_trend(int idx) const {
    if (idx == -1) {
      auto& top_trends = ind_1h.trends.price.top_trends;
      return top_trends.empty() ? TrendLine{} : top_trends[0];
    }

    auto top_trends = Trends::price_trends(ind_1h, idx).top_trends;
    return top_trends.empty() ? TrendLine{} : top_trends[0];
  }

  TrendLine rsi_trend(int idx) const {
    if (idx == -1) {
      auto& top_trends = ind_1h.trends.rsi.top_trends;
      return top_trends.empty() ? TrendLine{} : top_trends[0];
    }

    auto top_trends = Trends::rsi_trends(ind_1h, idx).top_trends;
    return top_trends.empty() ? TrendLine{} : top_trends[0];
  }

  TrendLine ema21_trend(int idx) const {
    if (idx == -1) {
      auto& top_trends = ind_1h.trends.ema21.top_trends;
      return top_trends.empty() ? TrendLine{} : top_trends[0];
    }

    auto top_trends = Trends::ema21_trends(ind_1h, idx).top_trends;
    return top_trends.empty() ? TrendLine{} : top_trends[0];
  }
};

