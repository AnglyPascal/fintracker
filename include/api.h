#pragma once

#include "indicators.h"
#include "times.h"

#include <chrono>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <vector>

inline constexpr int MAX_OUTPUT_SIZE = 5000;
inline constexpr int API_TOKENS = 800;
inline constexpr int MAX_CALLS_MIN = 8;
inline constexpr int Ns = 3;

class TG {
 private:
  static std::set<int> seen_updates;

 public:
  static int send(const std::string& text);
  static int send_doc(const std::string& doc_name,
                      const std::string& caption = "");

  static int pin_message(int message_id);
  static int edit_msg(int message_id, const std::string& text);

  static std::tuple<bool, std::string, int> receive(int last_update_id);
};

struct APIKey {
  const std::string key;
  int daily_calls = 0;
  std::deque<TimePoint> call_timestamps = {};  // to enforce 8/min
};

class TD {
  using Result = std::vector<Candle>;

  std::vector<APIKey> keys;
  int idx = 0;

 public:
  const minutes interval;

 private:
  int try_get_key();
  const std::string& get_key();

  Result api_call(const std::string& symbol, size_t output_size);

 public:
  TD(size_t n_tickers);

  Result time_series(const std::string& symbol, int n_days = 90);
  Candle real_time(const std::string& symbol);
};
