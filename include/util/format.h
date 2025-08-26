#pragma once

#include <concepts>
#include <format>
#include <string>

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

template <typename Arg>
inline std::string colored(std::string_view color, Arg&& arg) {
  return std::format("<span style='color: var(--color-{});'>{}</span>",  //
                     color, std::forward<Arg>(arg));
}

inline std::string colored(std::string_view color, double arg) {
  return std::format("<span style='color: var(--color-{});'>{:.2f}</span>",  //
                     color, arg);
}

enum HTMLTags {
  BOLD,
  UL,
  IT,
  EM,
};

inline std::string apply_tag(std::string str, HTMLTags tag) {
  switch (tag) {
    case BOLD:
      return std::format("<b>{}</b>", str);
    case UL:
      return std::format("<u>{}</u>", str);
    case IT:
      return std::format("<i>{}</i>", str);
    case EM:
      return std::format("<em>{}</em>", str);

    default:
      return str;
  }
}

inline std::string apply_tag(std::string str, const std::string& color) {
  return colored(color, str);
}

template <typename Str, typename... Tags>
  requires std::constructible_from<std::string, Str>
inline std::string tagged(Str _str, Tags... tags) {
  std::string str{_str};
  ((str = apply_tag(std::move(str), tags)), ...);
  return str;
}

template <typename T, typename... Tags>
inline std::string tagged(T t, Tags... tags) {
  auto str = to_str(t);
  ((str = apply_tag(std::move(str), tags)), ...);
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

  // construct from format string
  template <typename... Args>
  explicit fmt_string(std::string_view fmt, Args&&... args)
      : std::string(std::vformat(fmt, std::make_format_args(args...))) {}
};
