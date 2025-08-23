#pragma once

#include <chrono>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

using SysClock = std::chrono::system_clock;
using SysTimePoint = std::chrono::sys_time<std::chrono::seconds>;

using LocalTimePoint = std::chrono::local_time<std::chrono::seconds>;

inline constexpr std::string_view ny_tz = "America/New_York";

using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
using minutes = std::chrono::minutes;
using hours = std::chrono::hours;
using days = std::chrono::days;

inline constexpr minutes M_1{1}, M_5{5}, M_15{15}, M_30{30},  //
    H_1{hours{1}}, H_2{hours{2}}, H_4{hours{4}}, D_1{hours{6} + minutes{30}};

LocalTimePoint datetime_to_local(std::string_view datetime,
                                 std::string_view fmt = "%F %T",
                                 std::string_view timezone = ny_tz);

inline LocalTimePoint date_to_local(std::string_view datetime,
                                    std::string_view timezone = ny_tz) {
  return datetime_to_local(datetime, "%F", timezone);
}

std::string datetime_to_string(LocalTimePoint tp);

SysTimePoint now_utc_time(std::string_view timezone = ny_tz);
LocalTimePoint now_ny_time();

std::string closest_nyse_aligned_time(const std::string& ny_time_str);

std::pair<bool, minutes> market_status(minutes update_interval);

struct Timer {
  TimePoint start;
  Timer() : start{Clock::now()} {}
  double diff_ms() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - start)
        .count();
  }
  double diff_s() const {
    return std::chrono::duration<double>(Clock::now() - start).count();
  }
};

bool first_candle_in_interval(minutes interval, LocalTimePoint tp);
bool last_candle_in_interval(minutes interval, LocalTimePoint tp);

LocalTimePoint start_of_interval(LocalTimePoint tp, minutes interval);

constexpr size_t candles_per_day(minutes timeframe) {
  minutes day_mins = hours{6} + minutes{30};
  return (day_mins + timeframe - minutes{1}) / timeframe;
}
