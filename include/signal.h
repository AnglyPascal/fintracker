#pragma once

#include <string>
#include <unordered_map>
#include <vector>

enum class SignalType { None, Entry, Exit };

enum class Reason {
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

struct Metrics;

using signal_f = Reason (*)(const Metrics&);
using hint_f = std::string (*)(const Metrics&);

struct Signal {
  SignalType type = SignalType::None;

  std::vector<Reason> entry_reasons, exit_reasons;
  std::vector<std::string> entry_hints, exit_hints;

  std::string to_str(bool tg = false) const;
  std::string color() const;
  std::string color_bg() const;
  std::string emoji() const;

  Signal() noexcept = default;
  Signal(const Metrics& m) noexcept;

 private:
  bool has_signal() const;
  bool has_hints() const;
};

