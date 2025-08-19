#pragma once

#include "ind/candle.h"
#include "util/times.h"

#include <deque>
#include <mutex>
#include <string>
#include <vector>

struct APIKey {
  const std::string key;
  int daily_calls = 0;
  std::deque<TimePoint> call_timestamps = {};  // to enforce 8/min
};

inline constexpr int MAX_OUTPUT_SIZE = 5000;

class TD {
  std::vector<APIKey> keys;
  int idx = 0;
  std::mutex mtx;

 public:
  const minutes interval;

 private:
  int try_get_key();
  std::string get_key();

  TimeSeriesRes api_call(const std::string& symbol,
                         minutes timeframe,
                         size_t output_size = MAX_OUTPUT_SIZE);

 public:
  TD(size_t n_tickers);

  TimeSeriesRes time_series(const std::string& symbol,
                            minutes timeframe = H_1) noexcept;
  RealTimeRes real_time(const std::string& symbol,
                        minutes timeframe = H_1) noexcept;
  LocalTimePoint latest_datetime() noexcept;
};
