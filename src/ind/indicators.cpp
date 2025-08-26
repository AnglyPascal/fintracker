#include "ind/indicators.h"

#include <cassert>
#include <numeric>

EMA::EMA(const std::vector<Candle>& candles, int period) noexcept
    : values(candles.size()), period(period) {
  double sma = 0;
  for (int i = 0; i < period; i++) {
    sma = (sma * i + candles[i].price()) / (i + 1);
    values[i] = sma;
  }

  auto alpha = 2.0 / (period + 1);
  for (size_t i = period; i < candles.size(); i++) {
    values[i] = (candles[i].price() - values[i - 1]) * alpha + values[i - 1];
  }
}

EMA::EMA(const std::vector<double>& prices, int period) noexcept
    : values(prices.size()), period(period) {
  double sma = 0;
  for (int i = 0; i < period; i++) {
    sma = sma * i / (i + 1) + prices[i] / (i + 1);
    values[i] = sma;
  }

  auto alpha = 2.0 / (period + 1);
  for (size_t i = period; i < prices.size(); i++) {
    values[i] = (prices[i] - values[i - 1]) * alpha + values[i - 1];
  }
}

void EMA::push_back(const Candle& candle) noexcept {
  return push_back(candle.price());
}

void EMA::push_back(double price) noexcept {
  auto alpha = 2.0 / (period + 1);
  auto last = values.back();
  values.push_back((price - last) * alpha + last);
}

RSI::RSI(const std::vector<Candle>& candles, int period) noexcept
    : values(), period(period) {
  if (candles.size() < size_t(period + 1))
    return;

  values.reserve(candles.size());
  last_price = candles[0].price();
  values.push_back(
      std::numeric_limits<double>::quiet_NaN());  // first candle has no delta

  // calculate initial gain/loss values
  for (int i = 1; i <= period; ++i) {
    double change = candles[i].price() - candles[i - 1].price();
    double gain = change > 0 ? change : 0.0;
    double loss = change < 0 ? -change : 0.0;
    gains.push_back(gain);
    losses.push_back(loss);
    values.push_back(std::numeric_limits<double>::quiet_NaN());
  }

  // initialize smoothed averages
  avg_gain = std::accumulate(gains.begin(), gains.end(), 0.0) / period;
  avg_loss = std::accumulate(losses.begin(), losses.end(), 0.0) / period;

  double rs = avg_loss == 0.0 ? std::numeric_limits<double>::infinity()
                              : avg_gain / avg_loss;
  values.back() = 100.0 - (100.0 / (1.0 + rs));

  // continue applying smoothing to the rest of the series
  for (size_t i = period + 1; i < candles.size(); ++i) {
    double change = candles[i].price() - candles[i - 1].price();
    double gain = change > 0 ? change : 0.0;
    double loss = change < 0 ? -change : 0.0;

    avg_gain = (avg_gain * (period - 1) + gain) / period;
    avg_loss = (avg_loss * (period - 1) + loss) / period;

    double rs = avg_loss == 0.0 ? std::numeric_limits<double>::infinity()
                                : avg_gain / avg_loss;
    values.push_back(100.0 - (100.0 / (1.0 + rs)));
    last_price = candles[i].price();
  }
}

void RSI::push_back(const Candle& candle) noexcept {
  double change = candle.price() - last_price;
  last_price = candle.price();

  double gain = change > 0 ? change : 0.0;
  double loss = change < 0 ? -change : 0.0;

  avg_gain = (avg_gain * (period - 1) + gain) / period;
  avg_loss = (avg_loss * (period - 1) + loss) / period;

  double rs = avg_loss == 0.0 ? std::numeric_limits<double>::infinity()
                              : avg_gain / avg_loss;
  values.push_back(100.0 - (100.0 / (1.0 + rs)));
}

bool RSI::rising() const {
  if (values.size() < 2)
    return false;
  return values[values.size() - 2] < values.back();
}

MACD::MACD(const std::vector<Candle>& candles,
           int fast,
           int slow,
           int signal) noexcept
    : macd_line{std::vector<double>(candles.size())},
      fast_ema{candles, fast},
      slow_ema{candles, slow}  //
{
  size_t n = candles.size();
  for (size_t i = 0; i < n; ++i)
    macd_line[i] = fast_ema.values[i] - slow_ema.values[i];

  signal_ema = EMA(macd_line, signal);
  auto& signal_line = signal_ema.values;
  for (size_t i = 0; i < n; ++i)
    histogram.push_back(macd_line[i] - signal_line[i]);
}

void MACD::push_back(const Candle& candle) noexcept {
  fast_ema.push_back(candle);
  slow_ema.push_back(candle);

  double macd = fast_ema.values.back() - slow_ema.values.back();
  macd_line.push_back(macd);

  signal_ema.push_back(macd);
  double signal_val = signal_ema.values.back();
  histogram.push_back(macd - signal_val);
}

void MACD::pop_back() noexcept {
  fast_ema.pop_back();
  slow_ema.pop_back();
  signal_ema.pop_back();

  macd_line.pop_back();
  histogram.pop_back();
}

constexpr double true_range(double prev_close, const Candle& c) {
  double high_low = c.high - c.low;
  double high_pc = std::abs(c.high - prev_close);
  double low_pc = std::abs(c.low - prev_close);
  return std::max({high_low, high_pc, low_pc});
}

ATR::ATR(const std::vector<Candle>& candles, int period) noexcept
    : values(candles.size()), period(period) {
  double total_tr = 0;
  for (int i = 1; i <= period; ++i) {
    total_tr += true_range(candles[i - 1].close, candles[i]);
    values[i] = total_tr / i;
  }
  values[0] = 0;

  for (size_t i = period + 1; i < candles.size(); ++i) {
    double prev_atr = values[i - 1];
    double tr = true_range(candles[i - 1].close, candles[i]);
    values[i] = (prev_atr * (period - 1) + tr) / period;
  }

  prev_close = candles.back().close;
}

void ATR::push_back(const Candle& candle) noexcept {
  double tr = true_range(prev_close, candle);
  double prev_atr = values.back();
  double atr = (prev_atr * (period - 1) + tr) / period;

  values.push_back(atr);
  prev_close = candle.close;
}

void Indicators::push_back(const Candle& candle) noexcept {
  candles.push_back(candle);

  _ema9.push_back(candle);
  _ema21.push_back(candle);
  _ema50.push_back(candle);
  _rsi.push_back(candle);
  _macd.push_back(candle);
  _atr.push_back(candle);

  trends = Trends{*this};
  signal = Signal{*this};
}

void Indicators::pop_back() noexcept {
  candles.pop_back();
  auto close = candles.back().close;

  _ema9.pop_back();
  _ema21.pop_back();
  _ema50.pop_back();
  _rsi.pop_back();
  _macd.pop_back();
  _atr.pop_back(close);

  trends = Trends{*this};
  signal = Signal{*this};
}

Signal Indicators::get_signal(int idx) const {
  return (sanitize(idx) == size() - 1) ? signal : Signal{*this, idx};
}
