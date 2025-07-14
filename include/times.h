#pragma once

#include <chrono>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

using SysClock = std::chrono::system_clock;
using SysTimePoint = std::chrono::sys_time<std::chrono::seconds>;

using LocalClock = std::chrono::local_time<std::chrono::seconds>;
using LocalTimePoint = LocalClock;

inline constexpr std::string_view kDefaultTZ = "America/New_York";

using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
using minutes = std::chrono::minutes;
using hours = std::chrono::hours;
using days = std::chrono::days;

inline constexpr minutes M_1(1), M_5(5), M_15(15), M_30(30), H_1(60),
    H_2(2 * 60), H_4(4 * 60), D_1(8 * 60);

LocalTimePoint datetime_to_local(std::string_view datetime,
                                 std::string_view fmt = "%F %T",
                                 std::string_view timezone = kDefaultTZ);

LocalTimePoint date_to_local(std::string_view datetime,
                             std::string_view timezone = kDefaultTZ);

SysTimePoint now_utc_time(std::string_view timezone = kDefaultTZ);

LocalTimePoint now_ny_time();

std::string closest_nyse_aligned_time(const std::string& ny_time_str);

std::pair<bool, minutes> market_status();
