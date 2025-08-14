#pragma once

#include "times.h"

#include <string>

struct IndicatorsConfig {
  static constexpr const char* name = "ind_config";
  static constexpr bool debug = true;

  double eps = 1e-6;

  size_t n_top_trends = 3;

  size_t price_trend_min_candles = 10;
  size_t price_trend_max_candles = 60;

  size_t rsi_trend_min_candles = 5;
  size_t rsi_trend_max_candles = 30;

  size_t ema21_trend_min_candles = 15;
  size_t ema21_trend_max_candles = 75;

  double stats_importance_kappa = 0.25;
};

struct PositionSizingConfig {
  static constexpr const char* name = "pos_config";
  static constexpr bool debug = true;

  double capital = 4000.0;
  std::string capital_currency = "GBP";
  double max_risk_pct = 0.015;

  double max_capital_per_position = 0.25;

  double stop_pct = 0.025;
  double stop_atr_multiplier = 3.0;

  double trailing_trigger_atr = 1.5;
  double trailing_atr_multiplier = 3.0;
  double trailing_stop_pct = 0.035;

  double swing_low_buffer = 0.002;
  double ema_stop_pct = 0.02;

  double entry_score_cutoff = 0.6;
  double entry_risk_cutoff = 6.0;
  double entry_confidence_cutoff = 0.6;

  int min_hold_days = 2;
  int max_hold_days = 15;

  int earnings_volatility_buffer = 3;

  mutable double capital_usd = -1.0;

  double max_risk_amount() const { return capital_usd * max_risk_pct; }
};

struct SupportResistanceConfig {
  static constexpr const char* name = "sr_config";
  static constexpr bool debug = true;

  size_t lookback_days = 30;
  size_t n_zones = 6;
  double min_zone_confidence = 0.5;

  double zone_proximity_threshold = 0.007;
  double strong_confidence_threshold = 0.75;

  double break_buffer = 0.01;

  size_t swing_window_1h = 3;
  double zone_width_1h = 0.004;
  size_t n_candles_in_zone_1h = 5;

  size_t swing_window_4h = 2;
  double zone_width_4h = 0.008;
  size_t n_candles_in_zone_4h = 5;

  size_t swing_window_1d = 2;
  double zone_width_1d = 0.015;
  size_t n_candles_in_zone_1d = 2;

  auto swing_window(minutes interval) const {
    if (interval == H_1)
      return swing_window_1h;
    if (interval == H_4)
      return swing_window_4h;
    return swing_window_1d;
  }

  auto zone_width(minutes interval) const {
    if (interval == H_1)
      return zone_width_1h;
    if (interval == H_4)
      return zone_width_4h;
    return zone_width_1d;
  }

  auto n_candles_in_zone(minutes interval) const {
    if (interval == H_1)
      return n_candles_in_zone_1h;
    if (interval == H_4)
      return n_candles_in_zone_4h;
    return n_candles_in_zone_1d;
  }

  auto n_lookback_candles(minutes interval) const {
    if (interval == H_1)
      return lookback_days * 7;
    if (interval == H_4)
      return lookback_days * 2 * 2;
    return lookback_days * 3;
  }
};

struct SignalConfig {
  static constexpr const char* name = "sig_config";
  static constexpr bool debug = true;

  double entry_min = 1.0;
  double exit_min = 1.0;
  double entry_threshold = 3.5;
  double exit_threshold = 3.0;
  double mixed_min = 1.2;
  double watchlist_threshold = 3.5;

  double score_entry_weight = 0.6;
  double score_curr_alpha = 0.7;
  double score_hint_weight = 0.7;
  double score_squash_factor = 0.3;
  double score_memory_lambda = 0.5;

  double stop_reason_importance = 0.8;
  double stop_hint_importance = 0.8;

  double entry_4h_score_confirmation = 7;
  double entry_1d_score_confirmation = 7;
  double exit_4h_score_confirmation = -5;
  double exit_1d_score_confirmation = -5;

  double score_mod_4h_1d_agree = 1.2;
  double score_mod_4h_1d_align = 1.1;
  double score_mod_4h_1d_conflict = 0.7;

  double mem_strength_threshold = 0.75;

  int stop_max_holding_days = 20;
  double stop_atr_proximity = 0.75;
};

struct APIConfig {
  static constexpr const char* name = "api_config";
  static constexpr bool debug = false;

  std::vector<std::string> td_api_keys = {};

  std::string tg_token;
  std::string tg_chat_id;
  std::string tg_user;
};

struct Config {
  bool debug_en = false;
  int port = 8000;

  bool tg_en = true;
  bool plot_en = true;

  bool replay_en = false;
  bool replay_clear = false;

  double speed = 0.0;

  APIConfig api_config;
  IndicatorsConfig ind_config;
  PositionSizingConfig sizing_config;
  SupportResistanceConfig sr_config;
  SignalConfig sig_config;

  Config();
  void read_args(int argc, char* argv[]);
  void update();
};

inline Config config;
