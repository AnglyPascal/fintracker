#pragma once

#include "util/times.h"

#include <map>
#include <string>

enum class Confidence { Low, Medium, High };

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

enum class FilterAlignment {
  StronglyAligned,    // All timeframes bullish with high confidence
  ModeratelyAligned,  // Majority bullish, some neutral
  WeaklyAligned,      // Mixed bullish/neutral, low confidence
  Neutral,            // Mostly sideways/neutral across timeframes
  Conflicted,         // Clear opposing signals between timeframes
  StronglyConflicted  // Major bearish signals against bullish
};

struct Metrics;

struct Filters : public std::map<minutes::rep, std::vector<Filter>> {
  FilterAlignment alignment = FilterAlignment::Neutral;
  double score = 0.0;  // -1.0 to 1.0
  std::string rationale = "";

  Filters() = default;
  Filters(const Metrics& m);

 private:
  void classify();
};
