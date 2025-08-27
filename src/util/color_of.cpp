#include "core/positions.h"
#include "ind/candle.h"
#include "risk/sizing.h"
#include "sig/signal_types.h"
#include "util/format.h"
#include "util/times.h"

#include <string>

template <>
std::string color_of(const char* _str) {
  std::string str{_str};
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
    return "mist";

  if (str == "profit")
    return "green";
  if (str == "good")
    return "green";
  if (str == "semi-good")
    return "sage";

  if (str == "comment")
    return "gray";
  return "gray";
}

template <>
std::string color_of(StopContext sc) {
  if (sc == StopContext::NEW_POSITION)
    return "blue";
  if (sc == StopContext::EXISTING_INITIAL)
    return "coral";
  if (sc == StopContext::EXISTING_STANDARD)
    return "blue";
  if (sc == StopContext::EXISTING_TRAILING)
    return "green";
  if (sc == StopContext::SCALE_UP_ENTRY)
    return "sage";
  return "gray";
}

template <>
std::string color_of(Rating type) {
  switch (type) {
    case Rating::Entry:
      return color_of("good");
    case Rating::Exit:
      return color_of("bad");
    case Rating::Watchlist:
      return color_of("wishful");
    case Rating::Caution:
      return color_of("caution");
    case Rating::HoldCautiously:
      return color_of("semi-bad");
    case Rating::Mixed:
      return "white";
    case Rating::Skip:
      return "bg";
    default:
      return "gray";
  }
}

template <>
std::string color_of(Recommendation recom) {
  switch (recom) {
    case Recommendation::StrongBuy:
      return "green";
    case Recommendation::Buy:
      return "green";
    case Recommendation::WeakBuy:
      return "sage";
    case Recommendation::Avoid:
      return "red";
    default:
      return "gray";
  }
}

template <>
std::string color_of(MarketRegime r) {
  if (r == MarketRegime::TRENDING_UP)
    return "green";
  if (r == MarketRegime::CHOPPY_BULLISH)
    return "sage";
  if (r == MarketRegime::TRENDING_DOWN)
    return "red";
  if (r == MarketRegime::CHOPPY_BEARISH)
    return "coral";
  return "frost-aqua";
}

template <>
std::string color_of(SignalClass sc) {
  if (sc == SignalClass::Entry)
    return color_of("good");
  if (sc == SignalClass::Exit)
    return color_of("bad");
  return "gray";
}
