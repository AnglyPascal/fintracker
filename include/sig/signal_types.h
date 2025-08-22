#pragma once

#include "util/math.h"
#include "util/times.h"

#include <map>
#include <string>

enum class Severity { Urgent = 4, High = 3, Medium = 2, Low = 1 };
enum class Source { Price, Stop, EMA, RSI, MACD, Trend, SR, None };
enum class SignalClass { None, Entry, Exit };
enum class Confidence { Low, Medium, High };

struct Meta {
  Severity sev;
  Source src;
  SignalClass cls;
  std::string str = "";
};

enum class Rating {
  Exit,            // Strong sell signal
  Entry,           // Strong buy signal
  HoldCautiously,  // Already in trade, tightening stop
  Watchlist,       // Not a trade yet, but worth tracking
  Mixed,           // Conflicting signals
  None,            // No action
  OldWatchlist,    // Not a trade yet, but worth tracking
  Caution,         // Not a trade yet, but worth being cautious
  Skip,
};

enum class ReasonType {
  None,

  EmaCrossover,
  RsiCross50,
  PullbackBounce,
  MacdBullishCross,

  EmaCrossdown,
  RsiOverbought,
  MacdBearishCross,

  BrokeSupport,
  BrokeResistance,
};

enum class HintType {
  None,

  Ema9ConvEma21,
  RsiConv50,
  MacdRising,
  Pullback,

  RsiBullishDiv,
  RsiBearishDiv,

  MacdBullishDiv,
  // MacdBearishDiv,

  Ema9DivergeEma21,
  RsiDropFromOverbought,
  MacdPeaked,
  Ema9Flattening,

  // SR
  WithinTightRange,
  NearWeakSupport,
  NearStrongSupport,
  NearWeakResistance,
  NearStrongResistance,

  // Trends:

  PriceUp,
  PriceUpStrongly,
  Ema21Up,
  Ema21UpStrongly,
  RsiUp,
  RsiUpStrongly,

  PriceDown,
  PriceDownStrongly,
  Ema21Down,
  Ema21DownStrongly,
  RsiDown,
  RsiDownStrongly,
};

enum class StopHitType {
  None,
  StopLossHit,
  StopProximity,
  StopInATR,
  TimeExit,
};

template <typename T, T _none>
struct SignalType {
  using underlying_type = T;
  static constexpr T none = _none;

  T type = none;
  double score = 1.0;
  std::string desc = "";

 private:
  const Meta* meta = nullptr;

 public:
  SignalType() = default;
  SignalType(T type, double scr = 1.0, const std::string& desc = "");

  bool exists() const { return type != none; }
  auto severity() const { return meta ? meta->sev : Severity::Low; }
  auto severity_w() const { return static_cast<size_t>(severity()); }
  auto cls() const { return meta ? meta->cls : SignalClass::None; }
  auto str() const { return meta ? meta->str : ""; }
  auto source() const { return meta ? meta->src : Source::None; }

  bool operator<(const SignalType& other) const {
    return severity() < other.severity();
  }
};

using Reason = SignalType<ReasonType, ReasonType::None>;
using Hint = SignalType<HintType, HintType::None>;
using StopHit = SignalType<StopHitType, StopHitType::None>;

enum class Trend {
  StrongUptrend,
  ModerateUptrend,
  NeutralOrSideways,
  Caution,
  Bearish,
  None,
};

struct Filter {
  Trend trend;
  Confidence conf;
  std::string str;
  std::string desc;

  Filter() : trend{Trend::None}, conf{Confidence::Low}, str{""} {}
  Filter(Trend t,
         Confidence c,
         const std::string& str = "",
         const std::string& desc = "")
      : trend{t}, conf{c}, str{str}, desc{desc} {}
};

using Filters = std::map<minutes::rep, std::vector<Filter>>;

struct Confirmation {
  std::string str;

  Confirmation(const char* str) : str{str} {}
  Confirmation() : str{""} {}
};
