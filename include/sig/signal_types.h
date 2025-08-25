#pragma once

#include <string>

enum class Severity { Urgent = 4, High = 3, Medium = 2, Low = 1 };
enum class Source { Price, Stop, EMA, RSI, MACD, SR, None };
enum class SignalClass { None, Entry, Exit };

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

