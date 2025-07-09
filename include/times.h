#pragma once

#include <chrono>

using SysClock = std::chrono::system_clock;
using SysTimePoint = std::chrono::system_clock::time_point;
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

using minutes = std::chrono::minutes;
inline constexpr minutes M_1(1), M_5(5), M_15(15), M_30(30), H_1(60),
    H_2(2 * 60), H_4(4 * 60), D_1(8 * 60);

inline auto current_datetime() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::current_zone()->to_local(now);
}

