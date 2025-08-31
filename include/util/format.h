#pragma once

#include <concepts>
#include <format>
#include <string>
#include <type_traits>

inline constexpr std::string HASKELL = "haskell";
inline constexpr std::string DIFF = "diff";
inline constexpr std::string TEXT = "text";
inline constexpr std::string ELIXIR = "elixir";

enum class FormatTarget {
  None,
  Telegram,
  HTML,
};

template <FormatTarget target, typename T>
std::string to_str(const T& t);

template <FormatTarget target, typename T, typename S>
std::string to_str(const T& t, const S& s);

template <FormatTarget target, typename T, typename S, typename U>
std::string to_str(const T& t, const S& s, const U& u);

template <typename T>
std::string to_str(const T& t);

template <typename Str>
  requires std::constructible_from<std::string, Str>
std::string to_str(const Str& str) {
  return std::string{str};
}

template <FormatTarget target = FormatTarget::None>
inline std::string join(auto start, auto end, std::string sep = ", ") {
  std::string result;

  for (auto it = start; it != end; it++) {
    if constexpr (target == FormatTarget::None)
      result += to_str(*it);
    else
      result += to_str<target>(*it);

    auto _end = end;
    if (it != --_end)
      result += sep;
  }

  return result;
}

enum HTMLTags {
  BOLD,
  UL,
  IT,
  COLORED,
  NORMAL,
};

constexpr std::string __apply_tag(std::string str, HTMLTags tag) {
  switch (tag) {
    case BOLD:
      return "<b>" + str + "</b>";
    case UL:
      return "<u>" + str + "</u>";
    case IT:
      return "<i>" + str + "</i>";
    default:
      return str;
  }
}

constexpr std::string __apply_tag(std::string str, std::string_view color) {
  return std::format("<span style='color: var(--color-{});'>{}</span>",  //
                     color, str);
}

template <typename T, typename... Tags>
constexpr std::string tagged(const T& t, Tags... tags) {
  auto str = to_str(t);
  ((str = __apply_tag(std::move(str), tags)), ...);
  return str;
}

struct fmt_string : public std::string {
  using std::string::operator=;

  // default copy/move
  fmt_string(const fmt_string&) = default;
  fmt_string(fmt_string&&) noexcept = default;
  fmt_string& operator=(const fmt_string&) = default;
  fmt_string& operator=(fmt_string&&) noexcept = default;

  // construct from std::string (implicit)
  fmt_string(const std::string& s) : std::string(s) {}
  fmt_string(std::string&& s) noexcept : std::string(std::move(s)) {}

  template <typename... Args>
  explicit fmt_string(std::string_view fmt, Args&&... args)
      : std::string(std::vformat(fmt, std::make_format_args(args...))) {}
};

template <typename T>
std::string_view color_of(T t);

template <>
constexpr std::string_view color_of(const char* s) {
  std::string_view str{s};

  if (str == "atr")
    return "sage";
  if (str == "support")
    return "teal";

  if (str == "risk")
    return "coral";
  if (str == "loss")
    return "red";
  if (str == "bad")
    return "red";
  if (str == "semi-bad")
    return "coral";

  if (str == "caution")
    return "yellow";
  if (str == "wishful")
    return "sage-teal";
  if (str == "neutral")
    return "lilac";

  if (str == "profit")
    return "green";
  if (str == "good")
    return "green";
  if (str == "semi-good")
    return "sage";

  if (str == "info")
    return "frost";
  if (str == "comment")
    return "gray";

  return "gray";
}
