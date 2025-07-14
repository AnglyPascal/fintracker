#pragma once

#include "times.h"

#include <string>
#include <unordered_map>
#include <vector>

enum class Severity { Urgent = 4, High = 3, Medium = 2, Low = 1 };

enum class Source { Price, Stop, EMA, RSI, MACD, None };

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

struct Reason {
  ReasonType type;
  Severity severity;
  Source src = Source::None;

  Reason(ReasonType type) : type{type}, severity{Severity::Low} {}
  Reason(ReasonType type, Severity severity) : type{type}, severity{severity} {}
  Reason(ReasonType type, Severity severity, Source src)
      : type{type}, severity{severity}, src{src} {}

  bool operator<(const Reason& other) const {
    return severity < other.severity;
  }
};

enum class HintType {
  None,

  // Entry hints
  Ema9ConvEma21,
  RsiConv50,
  MacdRising,

  PriceTrendingUp,
  Ema21TrendingUp,
  RsiTrendingUp,
  RsiTrendingUpStrongly,

  // Exit hints
  Ema9DivergeEma21,
  RsiDropFromOverbought,
  MacdPeaked,
  Ema9Flattening,
  StopProximity,
  StopInATR,

  PriceTrendingDown,
  Ema21TrendingDown,
  RsiTrendingDown,
  RsiTrendingDownStrongly,
};

struct Hint {
  HintType type;
  Severity severity;
  Source src = Source::None;

  Hint(HintType type) : type{type}, severity{Severity::Low} {}
  Hint(HintType type, Severity severity) : type{type}, severity{severity} {}
  Hint(HintType type, Severity severity, Source src)
      : type{type}, severity{severity}, src{src} {}

  bool operator<(const Hint& other) const { return severity < other.severity; }
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

using signal_f = Reason (*)(const Metrics&);
using hint_f = Hint (*)(const Metrics&);
using conf_f = Confirmation (*)(const Metrics&);

struct Signal {
  SignalType type = SignalType::None;

  std::vector<Reason> entry_reasons, exit_reasons;
  std::vector<Hint> entry_hints, exit_hints;
  std::vector<Confirmation> confirmations;

  std::string color() const;
  std::string color_bg() const;
  std::string emoji() const;

  Signal() noexcept = default;
  Signal(const Metrics& m) noexcept;
  Signal& operator=(const Signal& _) noexcept = default;

  bool has_signal() const { return type != SignalType::None; }
  bool has_hints() const { return !entry_hints.empty() || !exit_hints.empty(); }
  bool is_interesting() const;

 private:
  int entry_score = 0;
  int exit_score = 0;

  SignalType gen_signal(bool has_position) const;
};

