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
};

enum class SignalType {
  None,            // No action
                   //
  Entry,           // Strong buy signal
  Watchlist,       // Not a trade yet, but worth tracking
                   //
  Exit,            // Strong sell signal
  HoldCautiously,  // Already in trade, tightening stop
  Caution,         // Not a trade yet, but worth being cautious
                   //
  Mixed,           // Conflicting signals
};

enum class ReasonType {
  None,

  // Entry reasons
  EmaCrossover,
  RsiCross50,
  PullbackBounce,
  MacdHistogramCross,

  // Exit reasons
  EmaCrossdown,
  RsiOverbought,
  MacdBearishCross,

  TimeExit,
  StopLossHit,
};

inline const std::unordered_map<ReasonType, Meta> reason_meta = {
    {ReasonType::None,  //
     {Severity::Low, Source::EMA, SignalClass::Entry}},
    // Entry:
    {ReasonType::EmaCrossover,  //
     {Severity::High, Source::EMA, SignalClass::Entry}},
    {ReasonType::RsiCross50,  //
     {Severity::Medium, Source::RSI, SignalClass::Entry}},
    {ReasonType::PullbackBounce,  //
     {Severity::Urgent, Source::Price, SignalClass::Entry}},
    {ReasonType::MacdHistogramCross,  //
     {Severity::Medium, Source::MACD, SignalClass::Entry}},
    // Exit:
    {ReasonType::EmaCrossdown,  //
     {Severity::High, Source::EMA, SignalClass::Exit}},
    {ReasonType::RsiOverbought,  //
     {Severity::Medium, Source::RSI, SignalClass::Exit}},
    {ReasonType::MacdBearishCross,  //
     {Severity::High, Source::MACD, SignalClass::Exit}},
    {ReasonType::StopLossHit,  //
     {Severity::Urgent, Source::Stop, SignalClass::Exit}},
};

struct Reason {
  ReasonType type;

  SignalClass cls = SignalClass::None;
  Severity severity = Severity::Low;
  Source source = Source::None;

  Reason(ReasonType type) : type{type} {
    auto it = reason_meta.find(type);
    if (it == reason_meta.end())
      return;
    cls = it->second.cls;
    severity = it->second.sev;
    source = it->second.src;
  }

  bool operator<(const Reason& other) const {
    return severity < other.severity;
  }

  bool exists() const { return type != ReasonType::None; }
};

enum class HintType {
  None,

  // Entry hints
  Ema9ConvEma21,
  RsiConv50,
  MacdRising,

  // Exit hints
  Ema9DivergeEma21,
  RsiDropFromOverbought,
  MacdPeaked,
  Ema9Flattening,
  StopProximity,
  StopInATR,
};

inline const std::unordered_map<HintType, Meta> hint_meta = {
    {HintType::None,  //
     {Severity::Low, Source::None, SignalClass::None}},
    // Entry
    {HintType::Ema9ConvEma21,  //
     {Severity::Low, Source::EMA, SignalClass::Entry}},
    {HintType::RsiConv50,  //
     {Severity::Low, Source::RSI, SignalClass::Entry}},
    {HintType::MacdRising,  //
     {Severity::Medium, Source::MACD, SignalClass::Entry}},
    // Exit
    {HintType::Ema9DivergeEma21,  //
     {Severity::Low, Source::EMA, SignalClass::Exit}},
    {HintType::RsiDropFromOverbought,  //
     {Severity::Medium, Source::RSI, SignalClass::Exit}},
    {HintType::MacdPeaked,  //
     {Severity::Medium, Source::MACD, SignalClass::Exit}},
    {HintType::Ema9Flattening,  //
     {Severity::Low, Source::EMA, SignalClass::Exit}},
    {HintType::StopProximity,  //
     {Severity::High, Source::Stop, SignalClass::Exit}},
    {HintType::StopInATR,  //
     {Severity::High, Source::Stop, SignalClass::Exit}},
};

struct Hint {
  HintType type;

  SignalClass cls = SignalClass::None;
  Severity severity = Severity::Low;
  Source source = Source::None;

  Hint(HintType type) : type{type} {
    auto it = hint_meta.find(type);
    if (it == hint_meta.end())
      return;
    cls = it->second.cls;
    severity = it->second.sev;
    source = it->second.src;
  }

  bool operator<(const Hint& other) const { return severity < other.severity; }

  bool exists() const { return type != HintType::None; }
};

enum class TrendType {
  None,

  PriceTrendingUp,
  Ema21TrendingUp,
  RsiTrendingUp,
  RsiTrendingUpStrongly,

  PriceTrendingDown,
  Ema21TrendingDown,
  RsiTrendingDown,
  RsiTrendingDownStrongly,
};

struct Trend {
  TrendType type;
  Trend(TrendType type) : type{type} {}
};

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
  SignalType type = SignalType::None;

  std::vector<Reason> reasons;
  std::vector<Hint> hints;
  std::vector<Trend> trends;
  std::vector<Confirmation> confirmations;

  std::string emoji() const;

  Signal() noexcept = default;
  Signal(const Metrics& m) noexcept;
  Signal& operator=(const Signal& _) noexcept = default;

  bool has_signal() const { return type != SignalType::None; }
  bool has_hints() const { return !hints.empty(); }
  bool is_interesting() const;

 private:
  int entry_score = 0;
  int exit_score = 0;

  SignalType gen_signal(bool has_position) const;
};

