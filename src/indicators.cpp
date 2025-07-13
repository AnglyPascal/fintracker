#include "indicators.h"
#include "format.h"
#include "positions.h"
#include "times.h"

#include <spdlog/spdlog.h>

#include <cassert>
#include <iostream>
#include <numeric>

inline double true_range(double prev_close, const Candle& candle) {
  auto high = candle.high, low = candle.low;
  return std::max(
      {high - low, std::abs(high - prev_close), std::abs(low - prev_close)});
}

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

void EMA::add(const Candle& candle) noexcept {
  return add(candle.price());
}

void EMA::add(double price) noexcept {
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

void RSI::add(const Candle& candle) noexcept {
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
      signal_line{signal_ema.values},
      fast_period{fast},
      slow_period{slow},
      signal_period{signal},
      fast_ema{candles, fast},
      slow_ema{candles, slow}  //
{
  size_t n = candles.size();
  for (size_t i = 0; i < n; ++i)
    macd_line[i] = fast_ema.values[i] - slow_ema.values[i];

  signal_ema = EMA(macd_line, signal);
  for (size_t i = 0; i < n; ++i)
    histogram.push_back(macd_line[i] - signal_line[i]);
}

void MACD::add(const Candle& candle) noexcept {
  fast_ema.add(candle);
  slow_ema.add(candle);

  double macd = fast_ema.values.back() - slow_ema.values.back();
  macd_line.push_back(macd);

  signal_ema.add(macd);
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

ATR::ATR(const std::vector<Candle>& candles, int period) noexcept
    : period{period} {
  if (candles.size() < (size_t)period + 1)
    return;

  std::vector<double> tr;
  for (size_t i = candles.size() - period; i < candles.size(); ++i) {
    auto prev_close = candles[i - 1].close;
    tr.push_back(true_range(prev_close, candles[i]));
  }

  val = std::accumulate(tr.begin(), tr.end(), 0.0) / period;
  prev_close = candles.back().close;
}

void ATR::add(const Candle& candle) noexcept {
  prev_atr = {val, prev_close};

  double tr = true_range(prev_close, candle);
  val = (val * (period - 1) + tr) / period;
  prev_close = candle.close;
}

void ATR::pop_back() noexcept {
  auto [v, c] = prev_atr;
  val = v;
  prev_close = c;
  prev_atr = {0.0, 0.0};
}

Indicators::Indicators(std::vector<Candle>&& candles, minutes interval) noexcept
    : candles{std::move(candles)},
      interval{interval},
      ema9{this->candles, 9},
      ema21{this->candles, 21},
      ema50{this->candles, 50},
      rsi{this->candles},
      macd{this->candles},
      atr{this->candles},
      trends{*this}  //
{}

void Indicators::add(const Candle& candle) noexcept {
  candles.push_back(candle);

  ema9.add(candle);
  ema21.add(candle);
  ema50.add(candle);
  rsi.add(candle);
  macd.add(candle);
  atr.add(candle);

  trends = Trends{*this};
}

void Indicators::pop_back() noexcept {
  candles.pop_back();

  ema9.pop_back();
  ema21.pop_back();
  ema50.pop_back();
  rsi.pop_back();
  macd.pop_back();
  atr.pop_back();

  trends = Trends{*this};
}

inline Candle combine(auto start, auto end) {
  assert(start != end);
  Candle out;

  out.datetime = (end - 1)->datetime;  // start time of the group
  out.open = start->open;
  out.close = (end - 1)->close;
  out.volume = 0;
  out.high = start->high;
  out.low = start->low;

  for (auto it = start; it != end; it++) {
    out.high = std::max(out.high, it->high);
    out.low = std::min(out.low, it->low);
    out.volume += it->volume;
  }
  return out;
}

std::vector<Candle> downsample(const std::vector<Candle>& candles,
                               minutes source,
                               minutes target) {
  if (candles.empty())
    return {};

  assert(target.count() % source.count() == 0);

  std::vector<Candle> out, bucket;
  std::string current_day = candles.front().day();
  LocalTimePoint bucket_time;

  for (const auto& c : candles) {
    auto now = c.time();
    auto day = c.day();

    if (day != current_day) {
      if (!bucket.empty()) {
        out.push_back(combine(bucket.begin(), bucket.end()));
        bucket.clear();
      }
      current_day = day;
      bucket_time = now;
    }

    auto end = bucket_time + target;
    if (end <= now) {
      if (!bucket.empty()) {
        out.push_back(combine(bucket.begin(), bucket.end()));
        bucket.clear();
      }
      bucket_time += target;
    }

    bucket.push_back(c);
  }

  if (!bucket.empty())
    out.push_back(combine(bucket.begin(), bucket.end()));

  return out;
}

Metrics::Metrics(const std::string& symbol,
                 std::vector<Candle>&& candles,
                 minutes interval,
                 const Position* position) noexcept
    : symbol{symbol},
      candles{std::move(candles)},
      interval{interval},
      indicators_1h{downsample(this->candles, interval, H_1), H_1},  //
      stop_loss{indicators_1h, position},                            //
      position{position},                                            //
      new_day_intervals_passed{0}                                    //
{
  if (has_position())
    position->max_price_seen = std::max(position->max_price_seen, last_price());
}

bool Metrics::has_position() const {
  return position != nullptr && position->qty != 0;
}

bool Metrics::add(const Candle& candle, const Position* position) noexcept {
  candles.push_back(candle);
  new_day_intervals_passed++;

  auto add_to_ind = [&](auto& ind) {
    size_t skip = ind.interval / interval;
    if (new_day_intervals_passed % skip != 1)
      ind.pop_back();

    auto new_candle = combine(candles.end() - skip, candles.end());
    ind.add(new_candle);

    spdlog::trace("[td] {}: {}", symbol.c_str(), to_str(new_candle).c_str());
    return true;
  };

  bool added = add_to_ind(indicators_1h);

  this->position = position;
  if (position != nullptr)
    position->max_price_seen =
        std::max(position->max_price_seen, candle.price());

  stop_loss = StopLoss{indicators_1h, position};

  return added;
}

Candle Metrics::pop_back() noexcept {
  auto candle = candles.back();
  candles.pop_back();
  new_day_intervals_passed--;

  auto pop_from_ind = [&](auto& ind) {
    ind.pop_back();

    size_t skip = ind.interval / interval;
    if (new_day_intervals_passed % skip == 0)
      return;

    auto new_candle = combine(candles.end() - skip, candles.end());
    ind.add(new_candle);

    spdlog::trace("[td] {}: {}", symbol.c_str(), to_str(new_candle).c_str());
  };

  pop_from_ind(indicators_1h);
  stop_loss = StopLoss{indicators_1h, position};

  return candle;
}

Pullback Metrics::pullback(size_t lookback) const {
  if (candles.size() < lookback)
    lookback = candles.size();

  double high = 0.0;
  for (size_t i = candles.size() - lookback; i < candles.size(); ++i)
    high = std::max(high, candles[i].high);

  return {high, (high - last_price()) / high * 100.0};
}

StopLoss::StopLoss(const Indicators& ind, const Position* pos) noexcept {
  if (pos == nullptr || pos->qty == 0)
    return;

  auto& candles = ind.candles;
  auto& ema21 = ind.ema21.values;

  double price = candles.back().price();
  double atr = ind.atr.val;
  double entry_price = pos->px;
  double gain = price - entry_price;

  is_trailing = (gain > atr);  // trailing kicks in after ~1R move

  // Initial Stop
  if (!is_trailing) {
    constexpr size_t N = 10;
    auto n = std::min(N, candles.size());

    swing_low = std::numeric_limits<double>::max();
    for (size_t i = candles.size() - n; i < candles.size(); ++i)
      swing_low = std::min(swing_low, candles[i].low);

    auto buffer = 0.015 * price;
    ema_stop = ema21.back() - buffer;

    auto best = std::min(swing_low, ema_stop) * 0.999;
    atr_stop = entry_price - 2.0 * atr;

    final_stop = std::max(best, atr_stop);
    stop_pct = (entry_price - final_stop) / entry_price;
  }

  // Trailing Stop
  else {
    auto max_price = pos->max_price_seen;

    atr_stop = max_price - 1.5 * atr;

    constexpr size_t N = 8;
    auto n = std::min(N, candles.size());

    swing_low = std::numeric_limits<double>::max();
    for (size_t i = candles.size() - n; i < candles.size(); ++i)
      swing_low = std::min(swing_low, candles[i].low);

    swing_low *= 0.998;  // Slack below recent lows

    final_stop = std::max(swing_low, atr_stop);
    stop_pct = (price - final_stop) / price;
  }
}
