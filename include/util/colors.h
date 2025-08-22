#pragma once

#include <string_view>

// Reset code
inline constexpr std::string_view RESET = "\033[0m";

// Foreground colors
inline constexpr std::string_view BLACK = "\033[30m";
inline constexpr std::string_view RED = "\033[31m";
inline constexpr std::string_view GREEN = "\033[32m";
inline constexpr std::string_view YELLOW = "\033[33m";
inline constexpr std::string_view BLUE = "\033[34m";
inline constexpr std::string_view MAGENTA = "\033[35m";
inline constexpr std::string_view CYAN = "\033[36m";
inline constexpr std::string_view WHITE = "\033[37m";

inline constexpr std::string_view BRIGHT_BLACK = "\033[90m";
inline constexpr std::string_view BRIGHT_RED = "\033[91m";
inline constexpr std::string_view BRIGHT_GREEN = "\033[92m";
inline constexpr std::string_view BRIGHT_YELLOW = "\033[93m";
inline constexpr std::string_view BRIGHT_BLUE = "\033[94m";
inline constexpr std::string_view BRIGHT_MAGENTA = "\033[95m";
inline constexpr std::string_view BRIGHT_CYAN = "\033[96m";
inline constexpr std::string_view BRIGHT_WHITE = "\033[97m";

// Background colors
inline constexpr std::string_view BLACK_BG = "\033[40m";
inline constexpr std::string_view RED_BG = "\033[41m";
inline constexpr std::string_view GREEN_BG = "\033[42m";
inline constexpr std::string_view YELLOW_BG = "\033[43m";
inline constexpr std::string_view BLUE_BG = "\033[44m";
inline constexpr std::string_view MAGENTA_BG = "\033[45m";
inline constexpr std::string_view CYAN_BG = "\033[46m";
inline constexpr std::string_view WHITE_BG = "\033[47m";

inline constexpr std::string_view BRIGHT_BLACK_BG = "\033[100m";
inline constexpr std::string_view BRIGHT_RED_BG = "\033[101m";
inline constexpr std::string_view BRIGHT_GREEN_BG = "\033[102m";
inline constexpr std::string_view BRIGHT_YELLOW_BG = "\033[103m";
inline constexpr std::string_view BRIGHT_BLUE_BG = "\033[104m";
inline constexpr std::string_view BRIGHT_MAGENTA_BG = "\033[105m";
inline constexpr std::string_view BRIGHT_CYAN_BG = "\033[106m";
inline constexpr std::string_view BRIGHT_WHITE_BG = "\033[107m";
