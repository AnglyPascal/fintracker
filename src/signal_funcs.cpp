#include "config.h"
#include "indicators.h"
#include "signals.h"

const SignalConfig& sig_config = config.sig_config;

// Filters

std::vector<Filter> evaluate_daily_trend(const Indicators& ind) {
  std::vector<Filter> res;

  double slope = ind.ema21_trend(-1).slope();
  double r2 = ind.ema21_trend(-1).r2;
  double rsi = ind.rsi(-1);

  // Trend strength
  if (slope > 0.08 && r2 > 0.8 && rsi > 55)
    res.emplace_back(Trend::StrongUptrend, Confidence::High, "⇗");
  else if (slope > 0.02 && r2 > 0.6 && rsi > 50)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "↗");
  else if (rsi > 45)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "🡒");
  else
    res.emplace_back(Trend::Bearish, Confidence::High, "↘");

  // EMA21 alignment
  if (ind.price(-1) > ind.ema21(-1))
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "p⌃21");
  else if (ind.price(-1) > ind.ema21(-1) * 0.99)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p~21");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "p⌄21");

  // EMA50
  if (ind.price(-1) > ind.ema50(-1))
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p⌃50");
  else if (ind.price(-1) > ind.ema50(-1) * 0.99)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p~50");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "p⌄50");

  // RSI signal
  if (rsi >= 50 && rsi <= 65)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "r50-65");
  else if (rsi > 65)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "r>65");
  else if (rsi < 50 && rsi > 40)
    res.emplace_back(Trend::Caution, Confidence::Low, "r40-50");
  else
    res.emplace_back(Trend::Bearish, Confidence::High, "r<40");

  if (ind.atr(-1) / ind.price(-1) < 0.01)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "lowVol");

  return res;
}

Filter potential_base(const Indicators& ind_4h) {
  const int base_window = 6;  // last 6 candles (24 hours on 4H)
  int start = -base_window;

  // Price compression (range < 1.5 * ATR)
  double max_price = ind_4h.price(start);
  double min_price = ind_4h.price(start);
  for (int i = start + 1; i < -1; ++i) {
    max_price = std::max(max_price, ind_4h.price(i));
    min_price = std::min(min_price, ind_4h.price(i));
  }
  double price_range = max_price - min_price;
  double atr = ind_4h.atr(-1);
  bool tight_range = price_range < 1.5 * atr;

  // MACD histogram rising
  bool hist_rising =
      ind_4h.hist(-1) > ind_4h.hist(-2) && ind_4h.hist(-2) > ind_4h.hist(-3);

  // RSI stable or rising
  bool rsi_stable = ind_4h.rsi(-1) >= ind_4h.rsi(-2) - 2;

  // EMA21 trend flat or rising
  auto ema_trend = ind_4h.ema21_trend(-1);
  bool ema_flat_or_up = ema_trend.slope() >= -0.0001;

  // No lower lows (support holding)
  bool higher_lows = true;
  for (int i = start + 1; i < -1; ++i) {
    if (ind_4h.candles[i].low < ind_4h.candles[i - 1].low) {
      higher_lows = false;
      break;
    }
  }

  if (tight_range && hist_rising && rsi_stable && ema_flat_or_up && higher_lows)
    return {Trend::StrongUptrend, Confidence::High, "base+"};

  if ((tight_range && ema_flat_or_up) && (hist_rising || rsi_stable) &&
      higher_lows)
    return {Trend::ModerateUptrend, Confidence::Medium, "base~"};

  if (tight_range && !hist_rising && !higher_lows)
    return {Trend::NeutralOrSideways, Confidence::Low, "base-"};

  return {};
}

std::vector<Filter> evaluate_four_hour_trend(const Indicators& ind_4h) {
  std::vector<Filter> res;

  auto& ind = ind_4h;

  auto ema_trend = ind.ema21_trend(-1);
  auto rsi_trend = ind.rsi_trend(-1);

  double ema_slope = ema_trend.slope();
  double ema_r2 = ema_trend.r2;

  double rsi = ind.rsi(-1);
  double rsi_slope = rsi_trend.slope();
  double rsi_r2 = rsi_trend.r2;

  if (ema_slope > 0.10 && ema_r2 > 0.8 && rsi > 55 && rsi_slope > 0.1 &&
      rsi_r2 > 0.7)
    res.emplace_back(Trend::StrongUptrend, Confidence::High, "⇗");
  else if ((ema_slope > 0.03 && ema_r2 > 0.6 && rsi > 50) ||
           (rsi_slope > 0.05 && rsi_r2 > 0.6 && rsi > 50))
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "↗");
  else if (rsi > 45)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "🡒");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "↘");

  if (ind.hist(-1) > 0)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "h+");
  else if (ind.hist(-1) > ind.hist(-2))
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "h~");
  else
    res.emplace_back(Trend::Bearish, Confidence::Low, "h-");

  if (rsi > 65)
    res.emplace_back(Trend::StrongUptrend, Confidence::Low, "r>65");
  else if (rsi < 65 && rsi > 50)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "r50-65");
  else if (rsi < 50 && rsi > 40)
    res.emplace_back(Trend::Caution, Confidence::Medium, "r40-50");
  else
    res.emplace_back(Trend::Bearish, Confidence::Low, "r<40");

  res.emplace_back(potential_base(ind_4h));

  return res;
}

Filters evaluate_filters(const Metrics& m) {
  Filters res{
      .trends_4h = evaluate_four_hour_trend(m.ind_4h),
      .trends_1d = evaluate_daily_trend(m.ind_1d),
  };
  return res;
}

// Entry reasons

inline Reason ema_crossover_entry(const Indicators& m, int idx) {
  if (m.ema9(idx - 1) <= m.ema21(idx - 1) && m.ema9(idx) > m.ema21(idx))
    return ReasonType::EmaCrossover;
  return ReasonType::None;
}

inline Reason rsi_cross_50_entry(const Indicators& m, int idx) {
  if (m.rsi(idx - 1) < 50 && m.rsi(idx) >= 50)
    return ReasonType::RsiCross50;
  return ReasonType::None;
}

inline Reason pullback_bounce_entry(const Indicators& m, int idx) {
  bool dipped_below_ema21 = m.price(idx - 1) < m.ema21(idx - 1);
  bool recovered_above_ema21 = m.price(idx) > m.ema21(idx);
  bool ema9_above_ema21 = m.ema9(idx) > m.ema21(idx);

  if (dipped_below_ema21 && recovered_above_ema21 && ema9_above_ema21)
    return ReasonType::PullbackBounce;

  return ReasonType::None;
}

inline Reason macd_histogram_cross_entry(const Indicators& m, int idx) {
  if (m.hist(idx - 1) < 0 && m.hist(idx) >= 0)
    return ReasonType::MacdHistogramCross;
  return ReasonType::None;
}

// Exit reasons

inline Reason ema_crossdown_exit(const Indicators& m, int idx) {
  if (m.ema9(idx - 1) >= m.ema21(idx - 1) && m.ema9(idx) < m.ema21(idx))
    return ReasonType::EmaCrossdown;
  return ReasonType::None;
}

inline Reason macd_bearish_cross_exit(const Indicators& ind, int idx) {
  if (ind.macd(idx - 1) >= ind.macd_signal(idx - 1) &&
      ind.macd(idx) < ind.macd_signal(idx))
    return ReasonType::MacdBearishCross;
  return ReasonType::None;
}

// SR breaks
//
// Support/Resistance Reasons (actual signals)
inline Reason broke_support_exit(const Indicators& ind, int idx) {
  double current_close = ind.price(idx);
  double prev_close = ind.price(idx - 1);

  for (const auto& zone : ind.support.zones) {
    // Previous candle was above/in the zone
    if (prev_close >= zone.lo) {
      double break_threshold = zone.lo * (1 - 0.01);  // FIXME with config

      // Current candle closed below zone with minimum penetration
      if (current_close < break_threshold) {
        // Zone confidence filter - only signal on stronger zones
        // Volume confirmation (if available) - break should have higher
        // volume
        // if (ind.volume(idx) > ind.avg_volume(idx, 10)) {
        //   // 10-period avg volume
        //   return ReasonType::BrokeSupport;
        // }
        // If no volume data, still signal based on price action
        return ReasonType::BrokeSupport;
      }
    }
  }
  return ReasonType::None;
}

inline Reason broke_resistance_entry(const Indicators& ind, int idx) {
  double current_close = ind.price(idx);
  double prev_close = ind.price(idx - 1);

  for (const auto& zone : ind.resistance.zones) {
    // Previous candle was below/in the zone
    if (prev_close <= zone.hi) {
      double break_threshold = zone.hi * (1 + 0.01);  // FIXME

      // Current candle closed above zone with minimum penetration
      if (current_close > break_threshold) {
        // Zone confidence filter
        // Volume confirmation - breakout should have higher volume
        // if (ind.volume(idx) > ind.avg_volume(idx, 10)) {
        //   return ReasonType::BrokeResistance;
        // }
        // If no volume data, still signal
        return ReasonType::BrokeResistance;
      }
    }
  }
  return ReasonType::None;
}

inline constexpr signal_f reason_funcs[] = {
    // Entry
    ema_crossover_entry,
    rsi_cross_50_entry,
    pullback_bounce_entry,
    macd_histogram_cross_entry,
    broke_resistance_entry,
    // Exit
    ema_crossdown_exit,
    macd_bearish_cross_exit,
    broke_support_exit,
};

std::vector<Reason> reasons(const Indicators& ind, int idx) {
  std::vector<Reason> res;
  for (auto f : reason_funcs) {
    res.push_back(f(ind, idx));
  }
  return res;
}

// Entry Hints
inline Hint ema_converging_hint(const Indicators& m, int idx) {
  double prev_dist = std::abs(m.ema9(idx - 1) - m.ema21(idx - 1));
  double curr_dist = std::abs(m.ema9(idx) - m.ema21(idx));

  if (m.ema9(idx) < m.ema21(idx) && curr_dist < prev_dist &&
      curr_dist / m.ema21(idx) < 0.01) {
    return HintType::Ema9ConvEma21;
  }

  return HintType::None;
}

inline Hint rsi_approaching_50_hint(const Indicators& ind, int idx) {
  if (ind.rsi(idx - 1) < ind.rsi(idx) && ind.rsi(idx) > 45 && ind.rsi(idx) < 50)
    return HintType::RsiConv50;
  return HintType::None;
}

inline Hint macd_histogram_rising_hint(const Indicators& ind, int idx) {
  if (ind.hist(idx - 1) < ind.hist(idx) && ind.hist(idx) < 0)
    return HintType::MacdRising;
  return HintType::None;
}

inline Hint price_pullback_hint(const Indicators& m, int idx) {
  if (m.price(idx - 1) > m.ema21(idx - 1) && m.price(idx) < m.ema21(idx))
    return HintType::Pullback;
  return HintType::None;
}

inline Hint rsi_bullish_divergence(const Indicators& ind, int idx) {
  // Price made a lower low, RSI made a higher low -> bullish divergence
  if (ind.low(idx - 1) > ind.low(idx) && ind.rsi(idx - 1) < ind.rsi(idx) &&
      ind.rsi(idx - 1) < 40 && ind.rsi(idx) < 60)
    return HintType::RsiBullishDiv;

  return HintType::None;
}

inline Hint macd_bullish_divergence(const Indicators& ind, int idx) {
  // Price made a lower low, MACD made a higher low
  if (ind.low(idx) < ind.low(idx - 1) && ind.macd(idx) > ind.macd(idx - 1) &&
      ind.macd(idx - 1) < 0 && ind.macd(idx) < 0)
    return HintType::MacdBullishDiv;

  return HintType::None;
}

// Exit Hints

inline Hint ema_diverging_hint(const Indicators& m, int idx) {
  double prev_dist = std::abs(m.ema9(idx - 1) - m.ema21(idx - 1));
  double curr_dist = std::abs(m.ema9(idx) - m.ema21(idx));

  if (m.ema9(idx) > m.ema21(idx) && curr_dist > prev_dist &&
      curr_dist / m.ema21(idx) > 0.02) {
    return HintType::Ema9DivergeEma21;
  }

  return HintType::None;
}

inline Hint rsi_falling_from_overbought_hint(const Indicators& m, int idx) {
  double prev = m.rsi(idx - 1), now = m.rsi(idx);
  if (prev > 70 && now < prev && prev - now > 3.0)
    return HintType::RsiDropFromOverbought;
  return HintType::None;
}

inline Hint macd_histogram_peaking_hint(const Indicators& m, int idx) {
  if (m.hist(idx - 2) < m.hist(idx - 1) && m.hist(idx - 1) > m.hist(idx))
    return HintType::MacdPeaked;
  return HintType::None;
}

inline Hint ema_flattens_hint(const Indicators& m, int idx) {
  double slope = m.ema9(idx) - m.ema9(idx - 1);
  bool is_flat = std::abs(slope / m.ema9(idx)) < 0.001;

  double dist = std::abs(m.ema9(idx) - m.ema21(idx));
  bool is_close = dist / m.ema21(idx) < 0.01;

  if (is_flat && is_close)
    return HintType::Ema9Flattening;

  return HintType::None;
}

inline Hint rsi_bearish_divergence(const Indicators& ind, int idx) {
  // Price up but RSI down — loss of momentum
  if (ind.price(idx) > ind.price(idx - 1) && ind.rsi(idx) < ind.rsi(idx - 1) &&
      ind.rsi(idx) > 50)
    return HintType::RsiBearishDiv;

  return HintType::None;
}

// Trends

inline Hint price_trending(const Indicators& m, int idx) {
  auto best = m.price_trend(idx);

  if (best.slope() > 0.3 && best.r2 > 0.8)
    return HintType::PriceUpStrongly;
  if (best.slope() > 0.15)
    return HintType::PriceUp;
  if (best.slope() < -0.3 && best.r2 > 0.8)
    return HintType::PriceDownStrongly;
  if (best.slope() < -0.15)
    return HintType::PriceDown;

  return HintType::None;
}

inline Hint ema21_trending(const Indicators& m, int idx) {
  auto best = m.ema21_trend(idx);

  if (best.slope() > 0.3 && best.r2 > 0.8)
    return HintType::Ema21UpStrongly;
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return HintType::Ema21Up;
  if (best.slope() < -0.3 && best.r2 > 0.8)
    return HintType::Ema21DownStrongly;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return HintType::Ema21Down;

  return HintType::None;
}

inline Hint rsi_trending(const Indicators& m, int idx) {
  auto best = m.rsi_trend(idx);

  if (best.slope() > 0.3 && best.r2 > 0.85)
    return HintType::RsiUpStrongly;
  if (best.slope() > 0.15)
    return HintType::RsiUp;
  if (best.slope() < -0.3 && best.r2 > 0.85)
    return HintType::RsiDownStrongly;
  if (best.slope() < -0.15)
    return HintType::RsiDown;

  return HintType::None;
}

// SR Proximity

// Support/Resistance Hints (early warnings)
inline Hint near_support_hint(const Indicators& ind, int idx) {
  double current_price = ind.price(idx);

  for (const auto& zone : ind.support.zones) {
    if (zone.contains(current_price)) {
      if (ind.price(idx) <= ind.price(idx - 1)) {
        return zone.confidence > sig_config.sr_strong_confidence
                   ? HintType::NearStrongSupport
                   : HintType::NearWeakSupport;
      }
    } else if (current_price > zone.hi &&
               current_price <= zone.hi * (1 + 0.008)) {
      if (ind.price(idx) < ind.price(idx - 1)) {
        return zone.confidence > sig_config.sr_strong_confidence
                   ? HintType::NearStrongSupport
                   : HintType::NearWeakSupport;
      }
    }
  }
  return HintType::None;
}

inline Hint near_resistance_hint(const Indicators& ind, int idx) {
  double current_price = ind.price(idx);

  for (const auto& zone : ind.resistance.zones) {
    if (zone.contains(current_price)) {
      if (ind.price(idx) >= ind.price(idx - 1)) {
        return zone.confidence > sig_config.sr_strong_confidence
                   ? HintType::NearStrongResistance
                   : HintType::NearWeakResistance;
      }
    } else if (current_price < zone.lo &&
               current_price >= zone.lo * (1 - 0.008)) {
      if (ind.price(idx) > ind.price(idx - 1)) {
        return zone.confidence > sig_config.sr_strong_confidence
                   ? HintType::NearStrongResistance
                   : HintType::NearWeakResistance;
      }
    }
  }
  return HintType::None;
}

inline constexpr hint_f hint_funcs[] = {
    // Entry
    ema_converging_hint,
    rsi_approaching_50_hint,
    macd_histogram_rising_hint,
    price_pullback_hint,

    rsi_bullish_divergence,
    macd_bullish_divergence,

    // Exit
    ema_diverging_hint,
    rsi_falling_from_overbought_hint,
    macd_histogram_peaking_hint,
    ema_flattens_hint,

    rsi_bearish_divergence,

    // Trends
    price_trending,
    ema21_trending,
    rsi_trending,

    // SR
    near_support_hint,
    near_resistance_hint,
};

std::vector<Hint> hints(const Indicators& ind, int idx) {
  std::vector<Hint> res;
  for (auto f : hint_funcs) {
    res.push_back(f(ind, idx));
  }
  return res;
}

// Entry confirmations

Confirmation higher_timeframe_alignment_confirmation(const Metrics& metrics) {
  auto sig_1h = metrics.get_signal(H_1, -1);
  auto sig_4h = metrics.get_signal(H_4, -1);
  auto sig_1d = metrics.get_signal(D_1, -1);

  if (sig_1h.type == Rating::Entry) {
    if (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist)
      return "4h entry";

    if (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist)
      return "1d entry";
  }

  return "";
}

inline constexpr conf_f confirmation_funcs[] = {
    higher_timeframe_alignment_confirmation,
};

std::vector<Confirmation> confirmations(const Metrics& m) {
  std::vector<Confirmation> res;
  for (auto f : confirmation_funcs)
    res.push_back(f(m));
  return res;
}

void Indicators::get_stats() {
  Backtest bt{*this};

  for (auto& f : reason_funcs) {
    auto [r, s] = bt.get_stats<ReasonType>(f);
    reason_stats.try_emplace(r, s);
  }

  for (auto& f : hint_funcs) {
    auto [r, s] = bt.get_stats<HintType>(f);
    hint_stats.try_emplace(r, s);
  }
}

// Stop Loss tests
StopHit stop_loss_hits(const Metrics& m, const StopLoss& stop_loss) {
  if (!m.has_position())
    return StopHitType::None;

  auto days_held =
      std::chrono::floor<days>(std::chrono::floor<days>(now_ny_time()) -
                               std::chrono::floor<days>(m.position->tp));
  if (days_held.count() > sig_config.stop_max_holding_days)
    return StopHitType::TimeExit;

  auto price = m.last_price();
  if (price < stop_loss.final_stop)
    return StopHitType::StopLossHit;

  auto dist = price - stop_loss.final_stop;

  if (dist > 0 && dist < m.ind_1h.atr(-1) * sig_config.stop_atr_proximity)
    return StopHitType::StopProximity;

  // FIXME what's this?
  if (dist < 1.0 * stop_loss.atr_stop - stop_loss.final_stop)
    return StopHitType::StopInATR;

  return StopHitType::None;
}
