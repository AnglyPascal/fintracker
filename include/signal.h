#pragma once

#include <string>
#include <unordered_map>
#include <vector>

enum class Severity { Urgent = 0, High = 1, Medium = 2, Low = 3 };

enum class SignalType {
  Skip,            // Skip
  None,            // No action
                   //
  Entry,           // Strong buy signal
  Watchlist,       // Not a trade yet, but worth tracking
                   //
  Exit,            // Strong sell signal
  HoldCautiously,  // Already in trade, tightening stop
  Caution,         // Not a trade yet, but worth being cautious
                   //
  Mixed            // Conflicting signals
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

  Reason(ReasonType type) : type{type}, severity{Severity::Low} {}
  Reason(ReasonType type, Severity severity) : type{type}, severity{severity} {}

  bool operator<(const Reason& other) const {
    return severity < other.severity;
  }
};

struct Hint {
  std::string str;
  Severity severity;

  Hint(const char* str) : str{str}, severity{Severity::Low} {}
  Hint(const char* str, Severity severity) : str{str}, severity{severity} {}

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

struct Metrics;

using filter_f = Filter (*)(const Metrics&);
using signal_f = Reason (*)(const Metrics&);
using hint_f = Hint (*)(const Metrics&);

struct Signal {
  SignalType type = SignalType::None;

  std::vector<Reason> entry_reasons, exit_reasons;
  std::vector<Hint> entry_hints, exit_hints;

  std::string color() const;
  std::string color_bg() const;
  std::string emoji() const;

  Signal() noexcept = default;
  Signal(const Metrics& m) noexcept;

  bool has_signal() const;
  bool has_hints() const;

 private:
  int entry_score = 0;
  int exit_score = 0;

  SignalType gen_signal(bool has_position) const;
};

