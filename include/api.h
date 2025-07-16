#pragma once

#include "times.h"

#include <deque>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class TG {
 private:
  bool enabled = true;
  mutable std::set<int> seen_updates;

 public:
  TG(bool tg_enabled = true) : enabled{tg_enabled} {}

  int send(const std::string& text) const;
  int send_doc(const std::string& fname,
               const std::string& copy_name,
               const std::string& caption = "") const;

  int pin_message(int message_id) const;
  int edit_msg(int message_id, const std::string& text) const;
  int delete_msg(int message_id) const;

  std::tuple<bool, std::string, int> receive(int last_update_id) const;
};

struct APIKey {
  const std::string key;
  int daily_calls = 0;
  std::deque<TimePoint> call_timestamps = {};  // to enforce 8/min
};

struct Candle;

class TD {
  using Result = std::vector<Candle>;

  std::vector<APIKey> keys;
  int idx = 0;
  std::mutex mtx;

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
  LocalTimePoint latest_datetime();
};

bool wait_for_file(const std::string& path,
                   seconds freshness = seconds{120},
                   seconds timeout = seconds{30},
                   milliseconds poll_interval = milliseconds{500});

void notify_plot_daemon(const std::vector<std::string>& tickers);
