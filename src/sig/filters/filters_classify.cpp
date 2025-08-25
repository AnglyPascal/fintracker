#include "sig/filters.h"

struct TimeframeAssessment {
  int bullish_count = 0;
  int bearish_count = 0;
  int neutral_count = 0;
  double weighted_score = 0.0;  // -1 to 1
  Confidence max_confidence = Confidence::Low;

  TimeframeAssessment() = default;
  TimeframeAssessment(const std::vector<Filter>& filters);
};

TimeframeAssessment::TimeframeAssessment(const std::vector<Filter>& filters) {
  for (auto& f : filters) {
    double weight = f.conf == Confidence::High     ? 1.0
                    : f.conf == Confidence::Medium ? 0.6
                                                   : 0.3;

    if (f.trend == Trend::StrongUptrend || f.trend == Trend::ModerateUptrend) {
      bullish_count++;
      weighted_score += weight * (f.trend == Trend::StrongUptrend ? 1.0 : 0.6);
    } else if (f.trend == Trend::Bearish || f.trend == Trend::Caution) {
      bearish_count++;
      weighted_score -= weight * (f.trend == Trend::Bearish ? 1.0 : 0.5);
    } else if (f.trend == Trend::NeutralOrSideways) {
      neutral_count++;
      // Small penalty for neutral in trending market
      weighted_score -= weight * 0.1;
    }

    if (f.conf > max_confidence)
      max_confidence = f.conf;
  }

  // Normalize by number of filters
  if (!filters.empty())
    weighted_score /= filters.size();
}

void Filters::classify() {
  // Skip if no filters
  if (empty())
    return;

  // Assess each timeframe
  auto align = count(0) ? TimeframeAssessment{at(0)} : TimeframeAssessment{};
  auto tf_1h = count(H_1.count()) ? TimeframeAssessment{at(H_1.count())}
                                  : TimeframeAssessment{};
  auto tf_4h = count(H_4.count()) ? TimeframeAssessment{at(H_4.count())}
                                  : TimeframeAssessment{};
  auto tf_1d = count(D_1.count()) ? TimeframeAssessment{at(D_1.count())}
                                  : TimeframeAssessment{};

  // Weight: 1D (40%), 4H (30%), 1H (20%), Alignment (10%) for swing trading
  score = tf_1d.weighted_score * 0.4 + tf_4h.weighted_score * 0.3 +
          tf_1h.weighted_score * 0.2 + align.weighted_score * 0.1;

  // Count conflicts
  int conflict_count = 0;
  if (tf_1d.bullish_count > 0 && tf_1d.bearish_count > 0)
    conflict_count++;
  if (tf_4h.bullish_count > 0 && tf_4h.bearish_count > 0)
    conflict_count++;
  if (tf_1h.bullish_count > 0 && tf_1h.bearish_count > 0)
    conflict_count++;

  // Cross-timeframe conflicts (more serious)
  bool htf_ltf_conflict = false;
  if ((tf_1d.bearish_count > tf_1d.bullish_count && tf_1h.bullish_count > 0) ||
      (tf_1d.bullish_count > 0 && tf_1h.bearish_count > tf_1h.bullish_count))
    htf_ltf_conflict = true;

  // Major timeframe agreement
  bool all_bullish = tf_1d.bullish_count > 0 && tf_4h.bullish_count > 0 &&
                     tf_1h.bullish_count > 0 && tf_1d.bearish_count == 0 &&
                     tf_4h.bearish_count == 0;

  [[maybe_unused]]
  bool all_bearish = tf_1d.bearish_count > 0 && tf_4h.bearish_count > 0 &&
                     tf_1h.bearish_count > 0;

  // Determine alignment
  std::string rationale;

  if (all_bullish && score > 0.5) {
    alignment = FilterAlignment::StronglyAligned;
    rationale = "All timeframes bullish";
    if (tf_1d.max_confidence == Confidence::High)
      rationale += ", 1D high confidence";
  } else if (htf_ltf_conflict || conflict_count >= 2) {
    if (score < -0.2 || tf_1d.bearish_count >= 2) {
      alignment = FilterAlignment::StronglyConflicted;
      rationale = "Major HTF/LTF conflict";
      if (tf_1d.bearish_count >= 2)
        rationale += ", 1D bearish";
    } else {
      alignment = FilterAlignment::Conflicted;
      rationale = "Mixed signals across timeframes";
    }
  } else if (score > 0.3) {
    if (tf_1d.bullish_count > 0 && tf_4h.bullish_count > 0) {
      alignment = FilterAlignment::ModeratelyAligned;
      rationale = "HTF bullish alignment";
    } else {
      alignment = FilterAlignment::WeaklyAligned;
      rationale = "Some bullish bias";
    }
  } else if (score < -0.3) {
    alignment = FilterAlignment::StronglyConflicted;
    rationale = "Bearish bias";
    if (tf_1d.bearish_count > 0)
      rationale += ", 1D bearish";
  } else {
    // Check if genuinely neutral or just lacking signals
    int total_neutral =
        tf_1d.neutral_count + tf_4h.neutral_count + tf_1h.neutral_count;
    int total_filters = (count(H_1.count()) ? at(H_1.count()).size() : 0) +
                        (count(H_4.count()) ? at(H_4.count()).size() : 0) +
                        (count(D_1.count()) ? at(D_1.count()).size() : 0);

    if (total_neutral > total_filters * 0.5) {
      alignment = FilterAlignment::Neutral;
      rationale = "Sideways/consolidating";
    } else {
      alignment = FilterAlignment::WeaklyAligned;

      rationale = "Unclear direction";
    }
  }
}
