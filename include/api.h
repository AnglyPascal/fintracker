#pragma once

#include "candle.h"
#include "times.h"

#include <deque>
#include <mutex>
#include <set>
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
  const std::string& get_key();

  TimeSeriesRes api_call(const std::string& symbol,
                         minutes timeframe,
                         size_t output_size = MAX_OUTPUT_SIZE);

 public:
  TD(size_t n_tickers);

  double to_usd(double amount, const std::string& currency = "GBP") noexcept;

  TimeSeriesRes time_series(const std::string& symbol,
                            minutes timeframe = H_1) noexcept;
  RealTimeRes real_time(const std::string& symbol,
                        minutes timeframe = H_1) noexcept;
  LocalTimePoint latest_datetime() noexcept;
};

bool wait_for_file(const std::string& path,
                   seconds freshness = seconds{120},
                   seconds timeout = seconds{30},
                   milliseconds poll_interval = milliseconds{500});

