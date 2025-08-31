#pragma once

#include "util/times.h"

#include <iostream>
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

  size_t backtest_lookback_1h = 3000;
  size_t backtest_lookback_4h = 700;
  size_t backtest_lookback_1d = 400;
  double backtest_memory_decay = 0.75;

  size_t backtest_lookback(minutes inv) const {
    return inv == H_1   ? backtest_lookback_1h
           : inv == H_4 ? backtest_lookback_4h
                        : backtest_lookback_1d;
  }

  size_t memory_length(minutes interval) {
    return interval == H_1 ? 16 : interval == H_4 ? 12 : 8;
  }
};

struct RiskConfig {  // Rename from PositionSizingConfig
  static constexpr const char* name = "risk_config";
  static constexpr bool debug = true;

  // Capital
  double capital = 4000.0;
  std::string capital_currency = "GBP";
  mutable double to_usd_rate = -1.0;

  // Risk limits - Per position
  static constexpr double MAX_RISK_PER_POSITION = 0.01;      // 1% hard cap
  static constexpr double TARGET_RISK_PER_POSITION = 0.005;  // 0.5% standard

  // Portfolio limits
  static constexpr double MAX_PORTFOLIO_RISK = 0.06;  // 6% total
  static constexpr int MAX_POSITIONS = 12;

  // Stop loss parameters (moved from old config)
  double stop_atr_multiplier_base = 3.5;  // Will be adjusted by volatility
  double trailing_trigger_R = 1.0;        // Trigger at 1R profit
  double trailing_stop_pct = 0.035;       // For extreme cases

  // Profit target parameters
  double min_rr_ratio = 1.5;
  double target_rr_ratio = 2.0;
  double max_rr_ratio = 3.0;

  // Entry filters
  double entry_score_cutoff = 0.5;
  double entry_risk_cutoff = 8.0;  // Increased from 6.0 for wider stops
  double entry_conf_cutoff = 0.4;  // Lowered from 0.6 for more flexibility

  int min_hold_days = 2;
  int max_hold_days = 2;

  // Other
  int earnings_buffer_days = 5;

  double capital_usd() const { return capital * to_usd_rate; }
  double max_risk_amount() const {
    return capital_usd() * MAX_RISK_PER_POSITION;
  }
  double max_position_amount() const {
    return capital_usd() * 0.25;
  }  // Max 25% in one position
};

struct SupportResistanceConfig {
  static constexpr const char* name = "sr_config";
  static constexpr bool debug = true;

  size_t lookback_days = 30;
  size_t n_zones = 6;
  double min_zone_conf = 0.5;

  double zone_proximity_threshold = 0.007;
  double strong_conf_threshold = 0.75;

  double break_buffer = 0.01;

  size_t swing_window_1h = 3;
  size_t swing_window_4h = 2;
  size_t swing_window_1d = 2;

  double zone_width_1h = 0.004;
  double zone_width_4h = 0.008;
  double zone_width_1d = 0.015;

  size_t n_candles_in_zone_1h = 5;
  size_t n_candles_in_zone_4h = 5;
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

  double entry_threshold = 3.5;
  double exit_threshold = 3.0;
  double watchlist_threshold = 3.5;
  double mixed_min = 1.2;

  double score_entry_weight = 0.6;
  double score_hint_weight = 0.7;
  double score_squash_factor = 0.3;

  double stop_reason_importance = 0.8;
  double stop_hint_importance = 0.8;

  double stop_atr_proximity = 0.75;

  // Reasons configs
  int ema_cross_lookback = 6;
  int macd_cross_lookback = 3;
  int rsi_cross_lookback = 4;
  int pullback_bounce_lookback = 6;
  int pullback_scan_lookback = 8;
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
  static constexpr int PORT_DEFAULT = 8000;
  static constexpr int RELOAD_PORT_DEFAULT = 35729;

  bool debug_en = false;
  int port = PORT_DEFAULT;
  int reload_port = RELOAD_PORT_DEFAULT;

  bool tg_en = true;
  bool plot_en = true;

  bool replay_en = false;
  bool replay_paused = false;
  bool replay_clear = false;

  double speed = 0.0;

  size_t n_concurrency = 1;

  APIConfig api_config;
  IndicatorsConfig ind_config;
  RiskConfig risk_config;
  SupportResistanceConfig sr_config;
  SignalConfig sig_config;

  Config();
  void read_args(int argc, char* argv[]);
  void update();

  seconds update_interval() {
    if (speed == 0.0)
      return minutes{2};
    auto secs = static_cast<uint64_t>(60 * 2 / speed);
    return seconds{secs};
  }
};

inline Config config;
