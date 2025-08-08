#include "times.h"

#include <spdlog/spdlog.h>
#include <iostream>

using namespace std::chrono;

LocalTimePoint datetime_to_local(std::string_view datetime,
                                 std::string_view fmt,
                                 std::string_view timezone) {
  if (datetime == "") {
    spdlog::error("[time] empty datetime string");
    return {};
  }

  std::istringstream in{std::string(datetime)};
  in.exceptions(std::ios::failbit | std::ios::badbit);

  local_time<seconds> local;
  in >> parse(std::string(fmt), local);

  if (timezone == ny_tz) {
    return local;  // Already NY time
  }

  // interpret as time in local time zone, then convert to ny local time
  auto& db = get_tzdb();
  zoned_time from_zt{db.locate_zone(std::string(timezone)), local};
  zoned_time ny_zt{db.locate_zone(std::string(ny_tz)), from_zt.get_sys_time()};

  return floor<seconds>(ny_zt.get_local_time());
}

LocalTimePoint now_ny_time() {
  zoned_time now_zt{ny_tz, system_clock::now()};
  return floor<seconds>(now_zt.get_local_time());
}

SysTimePoint now_utc_time(std::string_view timezone) {
  const auto& db = get_tzdb();
  const auto* tz = db.locate_zone(std::string(timezone));
  zoned_time zt{tz, system_clock::now()};
  return floor<seconds>(zt.get_sys_time());
}

std::string closest_nyse_aligned_time(const std::string& ny_time_str) {
  std::istringstream ss(ny_time_str);
  LocalTimePoint input_local;
  ss >> parse("%F %T", input_local);
  if (ss.fail())
    return "1999-01-01 00:00:00";

  auto day_start = floor<days>(input_local);
  if (weekday{day_start} == Saturday || weekday{day_start} == Sunday)
    return "1999-01-01 00:00:00";

  LocalTimePoint market_open = day_start + hours{9} + minutes{30};
  LocalTimePoint last_slot = day_start + hours{15} + minutes{30};

  if (input_local <= market_open)
    return std::format("{:%F %T}", market_open);
  if (input_local >= day_start + hours{16})
    return std::format("{:%F %T}", last_slot);

  auto delta = duration_cast<seconds>(input_local - market_open);
  auto slot = static_cast<int>((delta.count() + 1800) / 3600);
  auto aligned = market_open + hours{slot};

  if (aligned > last_slot)
    aligned = last_slot;

  return std::format("{:%F %T}", aligned);
}

std::pair<bool, minutes> market_status(minutes interval) {
  auto interval_mins = interval.count();
  auto now = now_ny_time();
  auto today = floor<days>(now);
  auto time_of_day = now - today;

  weekday wd{today};
  if (wd == Saturday || wd == Sunday)
    return {false, minutes{-1}};

  constexpr auto market_open = hours{9} + minutes{30};
  constexpr auto market_close = hours{16};

  if (time_of_day < market_open) {
    auto time_until_open = duration_cast<minutes>(market_open - time_of_day);
    return {true, time_until_open};
  }

  if (time_of_day < market_close) {
    auto mins_since_midnight = duration_cast<minutes>(time_of_day).count();
    auto next_block =
        ((mins_since_midnight + interval_mins - 1) / interval_mins) *
        interval_mins;
    auto mins_until_next_block = next_block - mins_since_midnight;
    return {true, minutes{mins_until_next_block}};
  }

  // Market already closed today
  return {false, minutes{-1}};
}

bool last_candle_in_interval(minutes interval, LocalTimePoint tp) {
  auto start_of_day = floor<days>(tp) + hours{9} + minutes{30};
  auto time_of_day = floor<minutes>(tp - start_of_day);
  return time_of_day % interval == (interval - minutes{15});
}

bool first_candle_in_interval(minutes interval, LocalTimePoint tp) {
  auto start_of_day = floor<days>(tp) + hours{9} + minutes{30};
  auto time_of_day = floor<minutes>(tp - start_of_day);
  return time_of_day % interval == minutes{0};
}

LocalTimePoint start_of_interval(LocalTimePoint tp, minutes interval) {
  auto start_of_day = floor<days>(tp) + hours{9} + minutes{30};
  auto time_of_day = floor<minutes>(tp - start_of_day);
  auto floor = time_of_day - time_of_day % interval;
  return start_of_day + floor;
}
