#include "util/format.h"
#include "core/positions.h"
#include "ind/candle.h"
#include "risk/sizing.h"
#include "sig/signal_types.h"
#include "util/times.h"

#include <string>

template <>
std::string to_str(const int& v) {
  return std::to_string(v);
}

template <>
std::string to_str(const size_t& v) {
  return std::to_string(v);
}

template <>
std::string to_str(const double& v) {
  return std::format("{:.2f}", v);
}

template <>
std::string to_str(const fmt_string& v) {
  return std::string{v};
}

template <>
std::string to_str(const SysTimePoint& datetime) {
  return std::format("{:%F %T}", datetime);
}

template <>
std::string to_str(const LocalTimePoint& datetime) {
  return std::format("{:%F %T}", datetime);
}

template <>
std::string to_str(const Candle& candle) {
  auto& [_, open, high, low, close, volume] = candle;
  return std::format("{} {:.2f} {:.2f} {:.2f} {:.2f} {}",  //
                     candle.time(), open, high, low, close, volume);
}

template <>
std::string to_str(const Position& pos) {
  return std::format("{:.2f} @ {:.2f}", pos.qty, pos.px);
}

template <>
std::string to_str(const std::string& str) {
  return str;
}

template <>
std::string to_str(const Reason& r) {
  return r.str();
}

template <>
std::string to_str(const Hint& h) {
  return h.str();
}

template <>
std::string to_str(const Rating& type) {
  switch (type) {
    case Rating::Entry:
      return "entry";
    case Rating::Exit:
      return "exit";
    case Rating::Watchlist:
      return "watch";
    case Rating::Caution:
      return "caution";
    case Rating::HoldCautiously:
      return "hold";
    case Rating::Mixed:
      return "mixed";
    case Rating::Skip:
      return "skip";
    default:
      return "none";
  }
}

template <>
std::string to_str(const Score& scr) {
  return std::format("{:+}", Score::pretty(scr.final));
}

template <>
std::string to_str(const Recommendation& recom) {
  switch (recom) {
    case Recommendation::StrongBuy:
      return "strong buy";
    case Recommendation::Buy:
      return "buy";
    case Recommendation::WeakBuy:
      return "weak buy";
    case Recommendation::Avoid:
      return "avoid";
    default:
      return "";
  }
}

template <>
std::string to_str(const MarketRegime& r) {
  if (r == MarketRegime::TRENDING_UP)
    return "↑trending";
  if (r == MarketRegime::CHOPPY_BULLISH)
    return "↑choppy";
  if (r == MarketRegime::TRENDING_DOWN)
    return "↓trending";
  if (r == MarketRegime::CHOPPY_BEARISH)
    return "↓choppy";
  return "ranging";
}

template <>
std::string to_str(const LinearRegression& lr) {
  return std::format("y = {:.2f} * x + {:.2f}", lr.slope, lr.intercept);
}

template <>
std::string to_str(const TrendLine& tl) {
  std::ostringstream oss;
  oss << "period: " << tl.period << ", slope: " << tl.line.slope
      << ", r²: " << tl.r2;
  return oss.str();
}

template <>
std::string to_str(const StopContext& sc) {
  if (sc == StopContext::NEW_POSITION)
    return "new position";
  if (sc == StopContext::EXISTING_INITIAL)
    return "initial";
  if (sc == StopContext::EXISTING_STANDARD)
    return "standard";
  if (sc == StopContext::EXISTING_TRAILING)
    return "trailing";
  if (sc == StopContext::SCALE_UP_ENTRY)
    return "scale up";
  return "";
}

template <>
std::string to_str(const SignalClass& sc) {
  if (sc == SignalClass::Entry)
    return "entry";
  if (sc == SignalClass::Exit)
    return "exit";
  return "";
}
