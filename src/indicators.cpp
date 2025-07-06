#include "indicators.h"
#include "api.h"
#include "positions.h"
#include "signal.h"

#include <cassert>
#include <numeric>

/*
 * Things that need to be on the tg updates
 * */

double Candle::true_range(double prev_close) const {
  return std::max(
      {high - low, std::abs(high - prev_close), std::abs(low - prev_close)});
}

Candle Candle::combine(const std::vector<Candle>& group) {
  assert(!group.empty());
  Candle out;

  out.datetime = group.front().datetime;  // start time of the group
  out.open = group.front().open;
  out.close = group.back().close;
  out.volume = 0;
  out.high = group.front().high;
  out.low = group.front().low;

  for (auto const& c : group) {
    out.high = std::max(out.high, c.high);
    out.low = std::min(out.low, c.low);
    out.volume += c.volume;
  }
  return out;
}

std::string Candle::to_str() const {
  return std::format("{} {:.2f} {:.2f} {:.2f} {:.2f} {}", datetime, open, high,
                     low, close, volume);
}

std::string Candle::day() const {
  return datetime.substr(0, 10);
}

SysTimePoint Candle::time() const {
  std::tm t = {};
  std::istringstream ss(datetime);
  ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
  return std::chrono::system_clock::from_time_t(std::mktime(&t));
}

EMA::EMA(const std::vector<Candle>& candles, int period) noexcept
    : values(candles.size()), period(period) {
  double sma = 0;
  for (int i = 0; i < period; i++) {
    sma = (sma * i + candles[i].close) / (i + 1);
    values[i] = sma;
  }

  auto alpha = 2.0 / (period + 1);
  for (int i = period; i < candles.size(); i++) {
    values[i] = (candles[i].close - values[i - 1]) * alpha + values[i - 1];
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
  for (int i = period; i < prices.size(); i++) {
    values[i] = (prices[i] - values[i - 1]) * alpha + values[i - 1];
  }
}

void EMA::add(const Candle& candle) noexcept {
  return add(candle.close);
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
  last_close = candles[0].close;
  values.push_back(
      std::numeric_limits<double>::quiet_NaN());  // first candle has no delta

  // calculate initial gain/loss values
  for (int i = 1; i <= period; ++i) {
    double change = candles[i].close - candles[i - 1].close;
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
    double change = candles[i].close - candles[i - 1].close;
    double gain = change > 0 ? change : 0.0;
    double loss = change < 0 ? -change : 0.0;

    avg_gain = (avg_gain * (period - 1) + gain) / period;
    avg_loss = (avg_loss * (period - 1) + loss) / period;

    double rs = avg_loss == 0.0 ? std::numeric_limits<double>::infinity()
                                : avg_gain / avg_loss;
    values.push_back(100.0 - (100.0 / (1.0 + rs)));
    last_close = candles[i].close;
  }
}

void RSI::add(const Candle& candle) noexcept {
  double change = candle.close - last_close;
  last_close = candle.close;

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
    : macd_line(candles.size()),
      signal_line(signal_ema.values),
      fast_period(fast),
      slow_period(slow),
      signal_period(signal),
      fast_ema(candles, fast),
      slow_ema(candles, slow)  //
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
  signal_line.push_back(signal_val);
  histogram.push_back(macd - signal_val);
}

ATR::ATR(const std::vector<Candle>& candles, int period) noexcept
    : period{period} {
  if (candles.size() < period + 1)
    return;

  std::vector<double> tr;
  for (size_t i = candles.size() - period; i < candles.size(); ++i) {
    auto prev_close = candles[i - 1].close;
    tr.push_back(candles[i].true_range(prev_close));
  }

  val = std::accumulate(tr.begin(), tr.end(), 0.0) / period;
  prev_close = candles.back().close;
}

void ATR::add(const Candle& candle) noexcept {
  double tr = candle.true_range(prev_close);
  val = (val * (period - 1) + tr) / period;
  prev_close = candle.close;
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
  atr.add(candle);

  trends = Trends{*this};
}

std::vector<Candle> Metrics::downsample(std::chrono::minutes target) const {
  assert(target.count() % interval.count() == 0);

  std::vector<Candle> out, bucket;
  std::string current_day = candles.front().day();
  SysTimePoint bucket_time;

  for (auto const& c : candles) {
    auto now = c.time();
    auto day = c.day();

    if (day != current_day) {
      if (!bucket.empty()) {
        out.push_back(Candle::combine(bucket));
        bucket.clear();
      }
      current_day = day;
      bucket_time = now;
    }

    auto end = bucket_time + target;
    if (end <= now) {
      if (!bucket.empty()) {
        out.push_back(Candle::combine(bucket));
        bucket.clear();
      }
      bucket_time += target;
    }

    bucket.push_back(c);
  }

  if (!bucket.empty())
    out.push_back(Candle::combine(bucket));

  return out;
}

Metrics::Metrics(const std::string& symbol,
                 std::vector<Candle>&& candles,
                 minutes interval,
                 const Position* position) noexcept
    : symbol{symbol},
      candles{std::move(candles)},
      interval{interval},
      indicators_1d{std::move(downsample(D_1)), D_1},  //
      indicators_4h{std::move(downsample(H_4)), H_4},  //
      indicators_1h{std::move(downsample(H_1)), H_1},  //
      stop_loss{indicators_1h, position},              //
      position{position}                               //
{}

bool Metrics::has_position() const {
  return position != nullptr && position->qty != 0;
}

void Metrics::add(const Candle& candle, const Position* position) noexcept {
  candles.push_back(candle);

  for (auto& indicators : std::initializer_list<Indicators*>{
           &indicators_1h, &indicators_4h, &indicators_1d}) {
    size_t skip = indicators->interval / interval;
    if (candles.size() % skip == 0) {
      std::vector<Candle> group(candles.end() - skip, candles.end());
      indicators->add(Candle::combine(group));
    }
  }

  this->position = position;
  stop_loss = StopLoss{indicators_1h, position};
}

double Metrics::last_price() const {
  return candles.back().close;
}

Pullback Metrics::pullback(int lookback) const {
  if (candles.size() < lookback)
    lookback = candles.size();

  double high = 0.0;
  for (size_t i = candles.size() - lookback; i < candles.size(); ++i)
    high = std::max(high, candles[i].high);

  return {high, (high - last_price()) / high * 100.0};
}

const std::string& Metrics::last_updated() const {
  return candles.back().datetime;
}

StopLoss::StopLoss(const Indicators& ind, const Position* pos) noexcept {
  if (pos == nullptr || pos->qty == 0)
    return;

  auto& candles = ind.candles;
  auto& ema21 = ind.ema21.values;

  if (candles.size() < 5 || ema21.size() < 1)
    return;

  constexpr int N = 10;  // lookback window for swing low
  auto n = std::min<int>(N, candles.size());

  swing_low = std::numeric_limits<double>::max();
  for (int i = candles.size() - n; i < candles.size(); ++i)
    swing_low = std::min(swing_low, candles[i].low);

  auto buffer = 0.015 * candles.back().close;  // fallback buffer (1.5%)
  auto ema21_val = ema21.back();

  ema_stop = ema21_val - buffer;

  // Round down slightly to avoid triggering on noise
  auto best = std::min(swing_low, ema_stop) * 0.999;

  auto price = pos->price();
  stop_pct = (2 * ind.atr.val) / price;
  atr_stop = price - 2 * ind.atr.val;

  final_stop = std::max(best, atr_stop);
}
