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

  std::string day() const;
  SysTimePoint time() const;

  double true_range(double prev_close) const;
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
};

struct RSI {
  std::vector<double> values;

 private:
  int period;

  std::deque<double> gains;
  std::deque<double> losses;
  double last_close = 0.0;
  double avg_gain = 0.0;
  double avg_loss = 0.0;

 public:
  RSI(const std::vector<Candle>& candles, int period = 14) noexcept;
  void add(const Candle& candle) noexcept;
  bool rising() const;
};

struct MACD {
  std::vector<double> macd_line;
  const std::vector<double>& signal_line;
  std::vector<double> histogram;

 private:
  int fast_period = 12;
  int slow_period = 26;
  int signal_period = 9;

  EMA fast_ema;
  EMA slow_ema;
  EMA signal_ema;

 public:
  MACD(const std::vector<Candle>& candles,
       int fast = 12,
       int slow = 26,
       int signal = 9) noexcept;
  void add(const Candle& candle) noexcept;
};

struct ATR {
  double val = 0.0;

 private:
  const int period = 14;
  double prev_close = 0.0;

 public:
  ATR(const std::vector<Candle>& candles, int period = 14) noexcept;

  void add(const Candle& candle) noexcept;
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
  void plot(const std::string& symbol) const;
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
  const std::string& symbol;
  std::vector<Candle> candles;
  const minutes interval;

  Indicators indicators_1h;
  StopLoss stop_loss;
  const Position* position;

  Metrics(const std::string& symbol,
          std::vector<Candle>&& candles,
          minutes interval,
          const Position* position) noexcept;

  void add(const Candle& candle, const Position* position) noexcept;

  auto last_price() const { return candles.back().close; }
  auto last_updated() const { return candles.back().datetime; }

  Pullback pullback(size_t lookback = 360) const;
  bool has_position() const;

  void plot() const;
};

