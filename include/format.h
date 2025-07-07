#pragma once

#include <string>

// Reset code
inline constexpr std::string RESET = "\033[0m";

// Foreground colors
inline constexpr std::string BLACK = "\033[30m";
inline constexpr std::string RED = "\033[31m";
inline constexpr std::string GREEN = "\033[32m";
inline constexpr std::string YELLOW = "\033[33m";
inline constexpr std::string BLUE = "\033[34m";
inline constexpr std::string MAGENTA = "\033[35m";
inline constexpr std::string CYAN = "\033[36m";
inline constexpr std::string WHITE = "\033[37m";

inline constexpr std::string BRIGHT_BLACK = "\033[90m";
inline constexpr std::string BRIGHT_RED = "\033[91m";
inline constexpr std::string BRIGHT_GREEN = "\033[92m";
inline constexpr std::string BRIGHT_YELLOW = "\033[93m";
inline constexpr std::string BRIGHT_BLUE = "\033[94m";
inline constexpr std::string BRIGHT_MAGENTA = "\033[95m";
inline constexpr std::string BRIGHT_CYAN = "\033[96m";
inline constexpr std::string BRIGHT_WHITE = "\033[97m";

// Background colors
inline constexpr std::string BLACK_BG = "\033[40m";
inline constexpr std::string RED_BG = "\033[41m";
inline constexpr std::string GREEN_BG = "\033[42m";
inline constexpr std::string YELLOW_BG = "\033[43m";
inline constexpr std::string BLUE_BG = "\033[44m";
inline constexpr std::string MAGENTA_BG = "\033[45m";
inline constexpr std::string CYAN_BG = "\033[46m";
inline constexpr std::string WHITE_BG = "\033[47m";

inline constexpr std::string BRIGHT_BLACK_BG = "\033[100m";
inline constexpr std::string BRIGHT_RED_BG = "\033[101m";
inline constexpr std::string BRIGHT_GREEN_BG = "\033[102m";
inline constexpr std::string BRIGHT_YELLOW_BG = "\033[103m";
inline constexpr std::string BRIGHT_BLUE_BG = "\033[104m";
inline constexpr std::string BRIGHT_MAGENTA_BG = "\033[105m";
inline constexpr std::string BRIGHT_CYAN_BG = "\033[106m";
inline constexpr std::string BRIGHT_WHITE_BG = "\033[107m";

enum class FormatTarget {
  Telegram,
  Alert,
  Console,
  HTML,
  SignalEntry,
  SignalExit,
};

template <FormatTarget target, typename T>
std::string to_str(const T& t);

template <FormatTarget target, typename T, typename S>
std::string to_str(const T& t, const S& s);

template <typename T>
std::string to_str(const T& t);

