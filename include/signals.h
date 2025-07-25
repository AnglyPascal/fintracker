#pragma once

#include "times.h"

#include <string>
#include <unordered_map>
#include <vector>

enum class Severity { Urgent = 4, High = 3, Medium = 2, Low = 1 };
enum class Source { Price, Stop, EMA, RSI, MACD, None };
enum class SignalClass { None, Entry, Exit };

struct Meta {
  Severity sev;
  Source src;
  SignalClass cls;
  std::string str = "";
};

enum class Rating {
  Exit = 0,            // Strong sell signal
  Entry = 1,           // Strong buy signal
  HoldCautiously = 2,  // Already in trade, tightening stop
  Watchlist = 3,       // Not a trade yet, but worth tracking
  Mixed = 4,           // Conflicting signals
  None = 5,            // No action
  Caution = 6,         // Not a trade yet, but worth being cautious
};

enum class ReasonType {
  None,

  EmaCrossover,
  RsiCross50,
  PullbackBounce,
  MacdHistogramCross,

  EmaCrossdown,
  RsiOverbought,
  MacdBearishCross,

  TimeExit,
  StopLossHit,
};

enum class HintType {
  None,

  Ema9ConvEma21,
  RsiConv50,
  MacdRising,
  Pullback,

  Ema9DivergeEma21,
  RsiDropFromOverbought,
  MacdPeaked,
  Ema9Flattening,
  StopProximity,
  StopInATR,
};

enum class TrendType {
  None,

  PriceUp,
  Ema21Up,
  RsiUp,
  RsiUpStrongly,

  PriceDown,
  Ema21Down,
  RsiDown,
  RsiDownStrongly,
};

template <typename T, T none>
struct SignalType {
  T type;

 private:
  const Meta* meta;

 public:
  SignalType(T type);

  bool exists() const { return type != none; }
  auto severity() const { return meta ? meta->sev : Severity::Low; }
  auto cls() const { return meta ? meta->cls : SignalClass::None; }
  auto str() const { return meta ? meta->str : ""; }
  auto source() const { return meta ? meta->src : Source::None; }

  bool operator<(const SignalType& other) const {
    return severity() < other.severity();
  }
};

using Reason = SignalType<ReasonType, ReasonType::None>;
using Hint = SignalType<HintType, HintType::None>;
using Trend = SignalType<TrendType, TrendType::None>;

enum class Confidence {
  StrongUptrend,
  ModerateUptrend,
  NeutralOrSideways,
  Bearish,
};

struct Filter {
  Confidence confidence;
  std::string str;

  Filter(Confidence confidence) : confidence{confidence}, str{""} {}
  Filter(Confidence confidence, const std::string& str)
      : confidence{confidence}, str{str} {}
};

struct Confirmation {
  std::string str;

  Confirmation(const char* str) : str{str} {}
  Confirmation() : str{""} {}
};

struct Metrics;

struct Candle;
using filter_f = Filter (*)(const std::vector<Candle>& candles,
                            minutes interval);

std::pair<bool, std::string> filter(const std::vector<Candle>& candles,
                                    minutes interval);

using signal_f = Reason (*)(const Metrics&, int idx);
using hint_f = Hint (*)(const Metrics&, int idx);
using trend_f = Trend (*)(const Metrics&);
using conf_f = Confirmation (*)(const Metrics&);

struct Signal {
  Rating type = Rating::None;
  double score;

  std::vector<Reason> reasons;
  std::vector<Hint> hints;
  std::vector<Trend> trends;
  std::vector<Confirmation> confirmations;

  bool has_rating() const { return type != Rating::None; }
  bool has_reasons() const { return !reasons.empty(); }
  bool has_hints() const { return !hints.empty(); }

  bool is_interesting() const;
};

