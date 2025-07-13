#include "times.h"
#include "format.h"

#include <iostream>

using namespace std::chrono;

LocalTimePoint datetime_to_local(std::string_view datetime,
                                 std::string_view fmt,
                                 std::string_view timezone) {
  std::istringstream in{std::string(datetime)};
  in.exceptions(std::ios::failbit | std::ios::badbit);

  local_time<seconds> local;
  in >> parse(std::string(fmt), local);

  if (timezone == kDefaultTZ) {
    return local;  // Already NY time
  }

  // Interpret as time in another time zone, then convert to NY local time
  const auto& db = get_tzdb();
  const auto* from_tz = db.locate_zone(std::string(timezone));
  const auto* ny_tz = db.locate_zone(std::string(kDefaultTZ));
  zoned_time from_zt{from_tz, local};
  zoned_time ny_zt{ny_tz, from_zt.get_sys_time()};

  return floor<seconds>(ny_zt.get_local_time());
}

LocalTimePoint date_to_local(std::string_view datetime,
                             std::string_view timezone) {
  return datetime_to_local(datetime, "%F", timezone);
}

LocalTimePoint now_ny_time() {
  zoned_time now_zt{kDefaultTZ, system_clock::now()};
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

bool is_us_market_open_now() {
  auto local = now_ny_time();
  auto day_start = floor<days>(local);
  weekday wd{day_start};

  // Weekend check
  if (wd == Saturday || wd == Sunday)
    return false;

  // Time of day check
  auto tod = floor<minutes>(local.time_since_epoch() % days{1});
  int mins = duration_cast<minutes>(tod).count();

  constexpr int open_minutes = 9 * 60 + 30;
  constexpr int close_minutes = 16 * 60;

  return mins >= open_minutes && mins < close_minutes;
}

template <>
std::string to_str(const SysTimePoint& datetime) {
  return std::format("{:%F %T}", datetime);
}

template <>
std::string to_str(const LocalTimePoint& datetime) {
  return std::format("{:%F %T}", datetime);
}

