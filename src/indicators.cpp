#include "indicators.h"
#include "config.h"
#include "position_sizing.h"
#include "positions.h"
#include "times.h"

#include <spdlog/spdlog.h>

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
  auto& signal_line = signal_ema.values;
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

inline double true_range(double prev_close, const Candle& c) {
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

void ATR::add(const Candle& candle) noexcept {
  double tr = true_range(prev_close, candle);
  double prev_atr = values.back();
  double atr = (prev_atr * (period - 1) + tr) / period;

  values.push_back(atr);
  prev_close = candle.close;
}

Indicators::Indicators(std::vector<Candle>&& candles,
                       minutes interval) noexcept
    : candles{std::move(candles)},
      interval{interval},
      _ema9{this->candles, 9},
      _ema21{this->candles, 21},
      _ema50{this->candles, 50},
      _rsi{this->candles},
      _macd{this->candles},
      _atr{this->candles},
      trends{*this},
      support{*this},
      resistance{*this}  //
{
  get_stats();
  signal = gen_signal(-1);
  for (int i = -1 - (int)SignalMemory::MEMORY_LENGTH; i < -1; i++)
    memory.add(gen_signal(i));
}

void Indicators::add(const Candle& candle) noexcept {
  bool new_candle = candles.back().time() != candle.time();
  if (new_candle)
    memory.add(signal);

  candles.push_back(candle);

  _ema9.add(candle);
  _ema21.add(candle);
  _ema50.add(candle);
  _rsi.add(candle);
  _macd.add(candle);
  _atr.add(candle);

  trends = Trends{*this};
  signal = gen_signal(-1);
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
  signal = gen_signal(-1);
}

void Indicators::pop_memory() noexcept {
  memory.remove();
}

inline Candle combine(auto start, auto end) {
  assert(start != end);
  Candle out;

  out.datetime = start->datetime;  // start time of the group
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

std::vector<Candle> downsample(std::vector<Candle>& candles,
                               minutes source,
                               minutes target) {
  if (candles.empty())
    return {};

  assert(target.count() % source.count() == 0);

  std::vector<Candle> out;

  if (source == target) {
    out.insert(out.end(), candles.cbegin(), candles.cend());
    return out;
  }

  std::vector<Candle> bucket;
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

Metrics::Metrics(std::vector<Candle>&& candles,
                 minutes interval,
                 const Position* position) noexcept
    : candles{std::move(candles)},
      interval{interval},
      ind_1h{downsample(this->candles, interval, H_1), H_1},  //
      ind_4h{downsample(this->candles, interval, H_4), H_4},  //
      ind_1d{downsample(this->candles, interval, D_1), D_1} {
  update_position(position);
}

bool Metrics::has_position() const {
  return position != nullptr && position->qty != 0;
}

void Metrics::update_position(const Position* pos) {
  position = pos;
  if (pos == nullptr)
    return;

  for (auto it = candles.rbegin(); it != candles.rend(); it++) {
    if (it->time() < pos->tp)
      break;
    pos->max_price_seen = std::max(pos->max_price_seen, it->price());
  }
}

Candle latest_candle(std::vector<Candle>& candles,
                     minutes source,
                     minutes target) {
  if (candles.empty())
    return {};

  if (source == target)
    return candles.back();

  auto last_time = candles.back().time();
  auto start_time = start_of_interval(last_time, target);

  auto end = candles.end();
  auto start = end;
  while (start_time <= (start - 1)->time())
    start--;

  return combine(start, end);
}

bool Metrics::add(const Candle& candle, const Position* pos) noexcept {
  if (candles.back().time() == candle.time())
    candles.pop_back();
  candles.push_back(candle);

  auto add_to_ind = [&](auto& ind) {
    auto prev_time = ind.candles.back().time();
    auto curr_time = candle.time();

    auto target = ind.interval;
    bool new_candle = start_of_interval(prev_time, target) !=
                      start_of_interval(curr_time, target);

    if (!new_candle)
      ind.pop_back();

    ind.add(latest_candle(candles, interval, ind.interval));
    return new_candle;
  };

  auto new_candle = add_to_ind(ind_1h);
  add_to_ind(ind_4h);
  add_to_ind(ind_1d);

  update_position(pos);
  return new_candle;
}

Candle Metrics::rollback() noexcept {
  auto candle = candles.back();
  candles.pop_back();

  auto pop_from_ind = [&](auto& ind) {
    auto prev_time = ind.candles.back().time();
    auto curr_time = candle.time();

    bool complete_pop = prev_time == curr_time;
    ind.pop_back();

    if (complete_pop) {
      ind.pop_memory();
      return;
    }

    ind.add(latest_candle(candles, interval, ind.interval));
  };

  pop_from_ind(ind_1h);
  pop_from_ind(ind_4h);
  pop_from_ind(ind_1d);

  return candle;
}

StopLoss::StopLoss(const Metrics& m,
                   const PositionSizingConfig& config) noexcept {
  auto& ind = m.ind_1h;

  auto price = ind.price(-1);
  auto atr = ind.atr(-1);

  auto pos = m.position;
  auto entry_price = m.has_position() ? pos->px : price;
  auto max_price_seen = m.has_position() ? pos->max_price_seen : price;

  // trailing kicks in after ~1R move
  is_trailing =
      m.has_position() &&
      (max_price_seen > entry_price + config.trailing_trigger_atr * atr);

  auto n = std::min((size_t)config.swing_low_window, ind.candles.size());

  swing_low = std::numeric_limits<double>::max();
  for (int i = -n; i <= -1; ++i)
    swing_low = std::min(swing_low, ind.low(i));
  swing_low *= 1.0 - config.swing_low_buffer;

  double hard_stop = 0.0;

  // Initial Stop
  if (!is_trailing) {
    ema_stop = ind.ema21(-1) * (1.0 - config.ema_stop_pct);
    auto best = std::min(swing_low, ema_stop) * 0.999;

    atr_stop = entry_price - config.stop_atr_multiplier * atr;
    hard_stop = entry_price * (1.0 - config.stop_pct);

    final_stop = std::max({best, atr_stop, hard_stop});
    stop_pct = (entry_price - final_stop) / entry_price;
  }

  // Trailing Stop
  else {
    atr_stop = max_price_seen - config.trailing_atr_multiplier * atr;
    hard_stop = max_price_seen * (1.0 - config.trailing_stop_pct);

    final_stop = std::max({swing_low, atr_stop, hard_stop});
    stop_pct = (price - final_stop) / price;
  }

  spdlog::trace(
      "[stop] {} price={:.2f} entry={:.2f} "
      "atr_stop={:.2f} hard={:.2f} final={:.2f}",
      is_trailing, price, entry_price, atr_stop, hard_stop, final_stop);
}

Forecast Indicators::gen_forecast(int) const {
  return Forecast(signal, reason_stats, hint_stats);  // FIX ME
}
