#include "ind/calendar.h"
#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

#include <spdlog/spdlog.h>

inline auto& sig_config = config.sig_config;

// More lenient disqualification for swing trades
inline bool disqualify(auto& filters) {
  int strongBearish1D = 0;
  [[maybe_unused]] int mediumBearish1D = 0;
  int strongBearish4H = 0;

  for (auto& f : filters.at(D_1.count())) {
    if (f.trend == Trend::Bearish) {
      if (f.conf == Confidence::High)
        ++strongBearish1D;
      else
        ++mediumBearish1D;
    }
  }

  for (auto& f : filters.at(H_4.count())) {
    if (f.trend == Trend::Bearish && f.conf == Confidence::High)
      ++strongBearish4H;
  }

  // Only disqualify on strong 1D bearish trend
  if (strongBearish1D >= 2)
    return true;

  // Or multiple strong bearish signals across timeframes
  if (strongBearish1D >= 1 && strongBearish4H >= 2)
    return true;

  return false;
}

struct FilterBias {
  double net_bullish = 0.0;
  double net_bearish = 0.0;
  int strong_bullish = 0;
  int strong_bearish = 0;
  std::string key_signals;

  FilterBias(const Filters& filters) {
    std::vector<std::string> key_items;

    for (auto& [inv, filter_list] : filters) {
      auto tf_label = inv == H_1.count()   ? "1h"
                      : inv == H_4.count() ? "4h"
                      : inv == D_1.count() ? "1d"
                                           : "align";

      for (auto& f : filter_list) {
        double weight = (f.conf == Confidence::High)     ? 1.0
                        : (f.conf == Confidence::Medium) ? 0.6
                                                         : 0.3;

        if (f.trend == Trend::StrongUptrend ||
            f.trend == Trend::ModerateUptrend) {
          net_bullish += weight;
          if (f.conf == Confidence::High) {
            strong_bullish++;
            if (!f.str.empty())
              key_items.push_back(std::format("{}: {}", tf_label, f.str));
          }
        } else if (f.trend == Trend::Bearish) {
          net_bearish += weight;
          if (f.conf == Confidence::High) {
            strong_bearish++;
            if (!f.str.empty())
              key_items.push_back(std::format("{}: {}", tf_label, f.str));
          }
        }
      }
    }

    // Keep only top 3 most significant signals
    if (key_items.size() > 3)
      key_items.resize(3);

    key_signals = "";
    for (size_t i = 0; i < key_items.size(); ++i) {
      if (i > 0)
        key_signals += " ";
      key_signals += key_items[i];
    }
  }
};

inline int count_filters(auto& filters, Trend trend_type, Confidence min_conf) {
  return std::count_if(filters.begin(), filters.end(), [=](auto& f) {
    return f.trend == trend_type && f.conf >= min_conf;
  });
}

inline std::tuple<Rating, double, std::string> contextual_rating(
    auto& sig_1h,
    auto& sig_4h,
    auto& sig_1d,
    auto& stop_hit,
    auto& filters,
    bool has_position  //
) {
  Rating base_rating = sig_1h.type;
  double score_mod = 0.0;
  std::string rationale;

  FilterBias filter_bias{filters};

  if (base_rating == Rating::Entry) {
    rationale += std::format("1h {}. ", tagged("entry", "green", BOLD));

    // Check 1d confirmation first (most important for swing trades)
    if (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist) {
      rationale +=
          std::format("1d {} strongly. ", tagged("confirms", "green", BOLD));
      score_mod += 0.04;
    } else if (sig_1d.type == Rating::Exit || sig_1d.type == Rating::Caution) {
      rationale +=
          std::format("1d {} -> {}. ", tagged("conflicts", "red", BOLD),
                      tagged("downgrading", "yellow", IT));
      base_rating = Rating::Watchlist;
      score_mod -= 0.05;
    }

    // Then 4h confirmation
    if (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist) {
      rationale += std::format("4h {}. ", tagged("confirms", "green", IT));
      score_mod += 0.03;
    } else if (sig_4h.type == Rating::Exit && base_rating == Rating::Entry) {
      rationale += std::format("4h {}. ", tagged("conflicts", "red", IT));
      base_rating = Rating::Mixed;
      score_mod -= 0.03;
    }

    // Filter integration
    if (filter_bias.strong_bullish >= 3) {
      rationale +=
          std::format("filters: {} ({}). ", tagged("excellent", "green", BOLD),
                      filter_bias.key_signals);
      score_mod += 0.035;
    } else if (filter_bias.strong_bearish >= 2) {
      rationale +=
          std::format("filters: {} – caution. ", tagged("bearish", "red", IT));
      base_rating = Rating::Watchlist;
      score_mod -= 0.04;
    }

  } else if (base_rating == Rating::Watchlist) {
    rationale += std::format("1h {}. ", tagged("watchlist", "yellow"));

    if ((sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist) &&
        sig_4h.type == Rating::Entry && filter_bias.strong_bullish >= 2) {
      rationale += std::format("strong 4h+1d support -> {}. ",
                               tagged("upgrading", "green", BOLD));
      base_rating = Rating::Entry;
      score_mod += 0.035;
    } else if (sig_1d.type == Rating::Entry && filter_bias.net_bullish > 2) {
      rationale +=
          std::format("1d {} + filters -> {}. ", tagged("entry", "green"),
                      tagged("upgrade", "green"));
      base_rating = Rating::Entry;
      score_mod += 0.03;
    } else if (sig_1d.type == Rating::Exit || filter_bias.strong_bearish >= 2) {
      rationale += std::format("htf {}. ", tagged("bearish", "red", IT));
      base_rating = Rating::Caution;
      score_mod -= 0.03;
    }

  } else if (base_rating == Rating::Exit) {
    rationale += std::format("1h {}. ", tagged("exit", "red", BOLD));

    if (sig_1d.type == Rating::Exit || sig_4h.type == Rating::Exit) {
      rationale +=
          std::format("htf {} exit. ", tagged("confirms", "red", BOLD));
      score_mod -= 0.06;
    } else if (sig_1d.type == Rating::Entry && !has_position) {
      rationale += std::format("1d bullish -> {}. ",
                               tagged("downgrading exit", "yellow", IT));
      base_rating = Rating::Caution;
      score_mod += 0.04;
    }

    if (filter_bias.strong_bearish >= 2) {
      rationale +=
          std::format("filters {}. ", tagged("confirm bearish", "red", BOLD));
      score_mod -= 0.025;
    }

  } else if (base_rating == Rating::None || base_rating == Rating::Mixed) {
    if (sig_1d.type == Rating::Entry && filter_bias.strong_bullish >= 2) {
      rationale += std::format("1h {}, but 1d+filters {}. ",   //
                               tagged("neutral", "blue", IT),  //
                               tagged("bullish", "green"));
      base_rating = Rating::Watchlist;
      score_mod += 0.025;
    } else if (sig_4h.type == Rating::Entry && sig_1d.type != Rating::Exit) {
      rationale += std::format("1h {}, 4h shows {}. ",         //
                               tagged("neutral", "blue", IT),  //
                               tagged("potential", "blue", IT));
      base_rating = Rating::Watchlist;
    } else if (sig_1d.type == Rating::Exit || filter_bias.strong_bearish >= 2) {
      rationale += std::format("htf {}. ", tagged("bearish", "red"));
      base_rating = Rating::Caution;
      score_mod -= 0.02;
    }
  }

  // Stop loss handling
  if (stop_hit.type == StopHitType::StopLossHit) {
    rationale += std::format("{}! ", tagged("STOP HIT", "red", BOLD, IT));
    base_rating = Rating::Exit;
  } else if (stop_hit.type == StopHitType::TimeExit) {
    rationale += std::format("{}. ", tagged("time exit", "red", IT));
    base_rating = Rating::Exit;
  } else if (stop_hit.type == StopHitType::StopProximity &&
             base_rating == Rating::Entry) {
    rationale += std::format("near {}. ", tagged("stop", "yellow", IT));
    base_rating = Rating::Mixed;
  }

  // Disqualification check
  if (stop_hit.type == StopHitType::None && disqualify(filters)) {
    rationale +=
        std::format("{} by filters. ", tagged("disqualified", "red", BOLD));
    return {Rating::Skip, score_mod, rationale};
  }

  // Position adjustments
  if (base_rating == Rating::Caution && has_position) {
    rationale +=
        std::format("hold position {}. ", tagged("cautiously", "yellow", IT));
    base_rating = Rating::HoldCautiously;
  }

  if (base_rating == Rating::Exit && !has_position) {
    rationale += std::format("no position to {}. ", tagged("exit", "gray"));
    base_rating = Rating::Caution;
  }

  return {base_rating, score_mod, rationale};
}

inline Score weighted_score(Score score_1h,
                            Score score_4h,
                            Score score_1d,
                            double mod) {
  auto [entry, exit, _] = score_1h;  // Use 1H scores as base

  // Weight: 1H primary (50%), 1D confirmation (30%), 4H bridge (20%)
  double final =
      score_1h.final * 0.5 + score_1d.final * 0.3 + score_4h.final * 0.2;

  // Strong alignment bonuses
  if (score_1h > 0 && score_1d > 0)
    final *= 1.2;  // 1H-1D alignment critical for swing trades

  if (score_1h > 0 && score_4h > 0 && score_1d > 0)
    final *= 1.3;  // All aligned

  // Conflict penalties
  if (score_1h > 0 && score_1d < -0.3)
    final *= 0.7;  // Major conflict with daily

  if (score_1h > 0 && score_4h < -0.5)
    final *= 0.85;  // 4H conflict less severe

  final += mod;
  return {entry, exit, final};
}

auto combined_forecast(auto fc_1h, auto fc_4h, auto fc_1d) {
  return !fc_1h.empty() ? fc_1h : !fc_4h.empty() ? fc_4h : fc_1d;
}

StopHit stop_loss_hits(const Metrics& m, const StopLoss& stop_loss);
Filters evaluate_filters(const Metrics& m);
std::vector<Confirmation> confirmations(const Metrics& m);

CombinedSignal::CombinedSignal(  //
    const Metrics& m,
    const StopLoss& sl,
    const Event& ev,
    int idx  //
) {
  auto sig_1h = m.ind_1h.get_signal(idx);
  auto sig_4h = m.ind_4h.get_signal(idx);
  auto sig_1d = m.ind_1d.get_signal(idx);

  stop_hit = stop_loss_hits(m, sl);
  forecast =
      combined_forecast(sig_1h.forecast, sig_4h.forecast, sig_1d.forecast);

  filters = evaluate_filters(m);
  auto [rating, mod, r_str] = contextual_rating(
      sig_1h, sig_4h, sig_1d, stop_hit, filters, m.has_position());

  type = rating;
  rationale = r_str;
  score = weighted_score(sig_1h.score, sig_4h.score, sig_1d.score, mod);

  if (type == Rating::Entry) {
    if (ev.is_earnings() && ev.days_until() >= 0 &&
        ev.days_until() < config.sizing_config.earnings_volatility_buffer) {
      type = Rating::Watchlist;
      rationale += std::format("Earnings proximity - . ",
                               tagged("waiting", "yellow", BOLD));
    }
  }
}
