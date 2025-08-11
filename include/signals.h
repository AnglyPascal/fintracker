#pragma once

#include "times.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

enum class Severity { Urgent = 4, High = 3, Medium = 2, Low = 1 };
enum class Source { Price, Stop, EMA, RSI, MACD, Trend, SR, None };
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
  OldWatchlist = 4,    // Not a trade yet, but worth tracking
  Mixed = 5,           // Conflicting signals
  None = 6,            // No action
  Caution = 7,         // Not a trade yet, but worth being cautious
  Skip = 8,
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

 private:
  const Meta* meta = nullptr;

 public:
  SignalType() = default;
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
using StopHit = SignalType<StopHitType, StopHitType::None>;

enum class Trend {
  StrongUptrend,
  ModerateUptrend,
  NeutralOrSideways,
  Caution,
  Bearish,
  None,
};

enum class Confidence {
  Low,
  Medium,
  High,
};

struct Filter {
  Trend trend;
  Confidence confidence;
  std::string str;

  Filter() : trend{Trend::None}, confidence{Confidence::Low}, str{""} {}
  Filter(Trend t, Confidence c, const std::string& desc = "")
      : trend{t}, confidence{c}, str{desc} {}
};

using Filters = std::unordered_map<decltype(std::declval<minutes>().count()),
                                   std::vector<Filter>>;

struct Confirmation {
  std::string str;

  Confirmation(const char* str) : str{str} {}
  Confirmation() : str{""} {}
};

struct Indicators;
struct Metrics;

using signal_f = Reason (*)(const Indicators&, int idx);
using hint_f = Hint (*)(const Indicators&, int idx);
using conf_f = Confirmation (*)(const Metrics&);

std::vector<Reason> reasons(const Indicators& ind, int idx);
std::vector<Hint> hints(const Indicators& ind, int idx);
std::vector<Confirmation> confirmations(const Metrics& m);
Filters evaluate_filters(const Metrics& m);

struct Signal {
  Rating type = Rating::None;
  double score;
  LocalTimePoint tp;

  std::vector<Reason> reasons;
  std::vector<Hint> hints;

  bool has_rating() const { return type != Rating::None; }
  bool has_reasons() const { return !reasons.empty(); }
  bool has_hints() const { return !hints.empty(); }

  bool is_interesting() const;
};

struct SignalMemory {
  std::deque<Signal> past;
  int memory_length = 0;

  SignalMemory(minutes interval) noexcept;

  void add(const Signal& sig) {
    past.push_back(sig);
    if (past.size() > (size_t)memory_length)
      past.pop_front();
  }

  void remove() {
    if (!past.empty())
      past.pop_back();
  }

  double score() const;

  int rating_score() const;
};

struct Stats;

struct Forecast {
  double expected_return = 0.0;
  double expected_drawdown = 0.0;
  int n_min_candles = 0;
  int n_max_candles = 0;
  double confidence = 0.0;

  Forecast() = default;
  Forecast(const Signal& sig, const Stats& stats);
};

struct CombinedSignal {
  Rating type = Rating::None;
  double score;

  StopHit stop_hit;
  Filters filters;
  std::vector<Confirmation> confirmations;

  Forecast forecast;
};
