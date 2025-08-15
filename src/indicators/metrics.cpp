#include "core/positions.h"
#include "ind/indicators.h"
#include "util/config.h"
#include "util/times.h"

#include <spdlog/spdlog.h>

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

inline auto downsample(auto& candles, minutes source, minutes target) {
  std::vector<Candle> out;

  if (candles.empty())
    return out;

  if (source == target)
    return candles;

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
      ind_1d{downsample(this->candles, interval, D_1), D_1}   //
{
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

inline Candle latest_candle(auto& candles, minutes source, minutes target) {
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

bool Metrics::push_back(const Candle& candle, const Position* pos) noexcept {
  if (candles.back().time() == candle.time())
    candles.pop_back();
  candles.push_back(candle);

  auto add_to_ind = [&](auto& ind) {
    auto prev_time = ind.time(-1);
    auto curr_time = candle.time();

    auto target = ind.interval;
    bool new_candle = prev_time != start_of_interval(curr_time, target);

    if (!new_candle)
      ind.pop_back();

    ind.push_back(latest_candle(candles, interval, ind.interval), new_candle);
    return new_candle;
  };

  auto new_candle = add_to_ind(ind_1h);
  add_to_ind(ind_4h);
  add_to_ind(ind_1d);

  update_position(pos);
  return new_candle;
}

void Metrics::rollback() noexcept {
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

    ind.push_back(latest_candle(candles, interval, ind.interval), false);
  };

  pop_from_ind(ind_1h);
  pop_from_ind(ind_4h);
  pop_from_ind(ind_1d);
}
