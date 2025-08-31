#include "ind/indicators.h"
#include "sig/filters.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

std::vector<Filter> evaluate_timeframe_alignment(const Metrics& m) {
  std::vector<Filter> res;

  auto sig_1h = m.get_signal(H_1);
  auto sig_4h = m.get_signal(H_4);
  auto sig_1d = m.get_signal(D_1);

  const auto& ind_1h = m.ind_1h;
  const auto& ind_4h = m.ind_4h;
  const auto& ind_1d = m.ind_1d;

  // === SIGNAL ALIGNMENT ===

  // Perfect alignment (all bullish)
  bool all_entry =
      (sig_1h.type == Rating::Entry && sig_4h.type == Rating::Entry &&
       sig_1d.type == Rating::Entry);
  bool all_bullish =
      (sig_1h.type == Rating::Entry || sig_1h.type == Rating::Watchlist) &&
      (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist) &&
      (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist);

  if (all_entry) {
    res.emplace_back(Trend::StrongUptrend, Confidence::High,
                     tagged("⇈", color_of("good"), BOLD),
                     "perfect alignment: all timeframes showing entry signals");
  } else if (all_bullish) {
    res.emplace_back(
        Trend::StrongUptrend, Confidence::Medium,
        tagged("↗↗↗", BOLD, color_of("good")),
        "strong alignment: all timeframes bullish (entry/watchlist)");
  }

  // Higher timeframe confirmations for 1h entries
  if (sig_1h.type == Rating::Entry) {
    if (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist) {
      Confidence conf = (sig_4h.type == Rating::Entry) ? Confidence::High
                                                       : Confidence::Medium;
      res.emplace_back(
          Trend::ModerateUptrend, conf, tagged("4h✓", color_of("semi-good")),
          std::format("4h timeframe confirms 1h entry: 4h shows {}",
                      sig_4h.type == Rating::Entry ? "entry" : "watchlist"));
    }

    if (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist) {
      Confidence conf = (sig_1d.type == Rating::Entry) ? Confidence::High
                                                       : Confidence::Medium;
      res.emplace_back(
          Trend::StrongUptrend, conf, tagged("1d✓", color_of("good")),
          std::format("daily timeframe confirms 1h entry: 1d shows {}",
                      sig_1d.type == Rating::Entry ? "entry" : "watchlist"));
    }
  }

  // Conflicting timeframes (warning signals)
  bool has_exits = (sig_4h.type == Rating::Exit || sig_1d.type == Rating::Exit);
  bool has_caution =
      (sig_4h.type == Rating::Caution || sig_1d.type == Rating::Caution ||
       sig_4h.type == Rating::HoldCautiously ||
       sig_1d.type == Rating::HoldCautiously);

  if (sig_1h.type == Rating::Entry && has_exits) {
    res.emplace_back(
        Trend::Caution, Confidence::High, tagged("htf⚠", color_of("bad")),
        "higher timeframe conflict: 1h entry but 4h/1d showing exits");
  } else if (sig_1h.type == Rating::Entry && has_caution) {
    res.emplace_back(
        Trend::Caution, Confidence::Medium, tagged("htf⚠", color_of("caution")),
        "higher timeframe caution: 1h entry but 4h/1d showing caution/hold");
  }

  // === STRUCTURAL ALIGNMENT ===

  // ema structure alignment
  bool daily_bull_structure = (ind_1d.ema21(-1) > ind_1d.ema50(-1) &&
                               ind_1d.price(-1) > ind_1d.ema21(-1));
  bool four_h_bull_structure = (ind_4h.ema21(-1) > ind_4h.ema50(-1) &&
                                ind_4h.price(-1) > ind_4h.ema21(-1));
  bool one_h_bull_structure = (ind_1h.ema9(-1) > ind_1h.ema21(-1) &&
                               ind_1h.price(-1) > ind_1h.ema9(-1));

  if (daily_bull_structure && four_h_bull_structure && one_h_bull_structure) {
    res.emplace_back(
        Trend::StrongUptrend, Confidence::High,
        tagged("ema⇈", BOLD, color_of("good")),
        "perfect ema alignment: all timeframes in bullish ema structure");
  } else if (daily_bull_structure && four_h_bull_structure) {
    res.emplace_back(Trend::ModerateUptrend, Confidence::High,
                     tagged("htf ema↗", color_of("good")),
                     "higher timeframe ema alignment: 4h and 1d both bullish");
  } else if (daily_bull_structure) {
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium,
                     tagged("1d ema↗", color_of("demi-good")),
                     "daily ema bullish: ema21 > ema50, price > ema21");
  }

  // Price position relative to key levels
  bool above_daily_21 = ind_1d.price(-1) > ind_1d.ema21(-1);
  bool above_daily_50 = ind_1d.price(-1) > ind_1d.ema50(-1);
  bool above_4h_21 = ind_4h.price(-1) > ind_4h.ema21(-1);

  if (above_daily_50 && above_daily_21 && above_4h_21) {
    res.emplace_back(Trend::ModerateUptrend, Confidence::High,
                     tagged("⊤ema", color_of("semi-good")),
                     "above key emas: price above 1d ema21/50 and 4h ema21");
  } else if (!above_daily_21 || !above_4h_21) {
    res.emplace_back(Trend::Caution, Confidence::Medium,
                     tagged("⊥ema", color_of("caution")),
                     std::format("below key emas: price below {}{}{}",
                                 !above_daily_21 ? "1d ema21" : "",
                                 !above_4h_21 && !above_daily_21 ? ", " : "",
                                 !above_4h_21 ? "4h ema21" : ""));
  }

  // === MOMENTUM ALIGNMENT ===

  // rsi alignment across timeframes
  bool daily_rsi_bull = ind_1d.rsi(-1) > 50;
  bool four_h_rsi_bull = ind_4h.rsi(-1) > 50;
  bool one_h_rsi_bull = ind_1h.rsi(-1) > 50;

  int bullish_rsi_count = daily_rsi_bull + four_h_rsi_bull + one_h_rsi_bull;

  if (bullish_rsi_count == 3) {
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium,
                     tagged("rsi↗", color_of("semi-good")),
                     "rsi momentum aligned: all timeframes above 50");
  } else if (bullish_rsi_count == 0) {
    res.emplace_back(Trend::Bearish, Confidence::Medium,
                     tagged("rsi↘", color_of("semi-bad")),
                     "rsi momentum bearish: all timeframes below 50");
  }

  // MACD momentum alignment
  bool daily_macd_bull = ind_1d.hist(-1) > 0;
  bool four_h_macd_bull = ind_4h.hist(-1) > 0;
  bool one_h_macd_bull = ind_1h.hist(-1) > 0;

  int bullish_macd_count = daily_macd_bull + four_h_macd_bull + one_h_macd_bull;

  if (bullish_macd_count == 3) {
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium,
                     tagged("macd↗", color_of("semi-good")),
                     "macd momentum aligned: all histograms above zero");
  } else if (bullish_macd_count == 0) {
    res.emplace_back(Trend::Bearish, Confidence::Medium,
                     tagged("macd↘", color_of("semi-bad")),
                     "macd momentum bearish: all histograms below zero");
  }

  return res;
}
