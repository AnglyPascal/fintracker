#pragma once

#include "times.h"

#include <string>

struct PositionSizingConfig {
  double capital = 4000.0;
  std::string capital_currency = "GBP";
  mutable double capital_usd = -1.0;

  double max_capital_per_position = 0.25;

  double max_risk_pct = 0.015;
  double stop_pct = 0.03;
  double stop_atr_multiplier = 3.0;

  int swing_low_window = 30;
  double swing_low_buffer = 0.002;
  double ema_stop_buffer = 0.015;

  double entry_score_cutoff = 0.6;
  double entry_risk_cutoff = 6.0;
  double entry_confidence_cutoff = 0.6;

  int min_hold_days = 2;
  int max_hold_days = 15;

  int earnings_volatility_buffer = 3;

  double max_risk_amount() const { return capital_usd * max_risk_pct; }

  PositionSizingConfig() = default;
  PositionSizingConfig(const std::string& path);
};

struct SupportResistanceConfig {
  int lookback_days = 30;
  int n_zones = 6;
  double min_zone_confidence = 3.0;

  double break_buffer = 0.01;

 private:
  int swing_window_1h = 3;
  double zone_width_1h = 0.004;
  int n_candles_in_zone_1h = 5;

  int swing_window_4h = 2;
  double zone_width_4h = 0.008;
  int n_candles_in_zone_4h = 5;

  int swing_window_1d = 2;
  double zone_width_1d = 0.015;
  int n_candles_in_zone_1d = 2;

 public:
  SupportResistanceConfig() = default;
  SupportResistanceConfig(const std::string& path);

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
};

struct Config {
  bool debug_en = false;

  bool tg_en = true;
  bool plot_en = true;

  bool replay_en = false;

  bool continuous_en = false;
  double speed = 1.0;

  PositionSizingConfig sizing_config;
  SupportResistanceConfig sr_config;

  Config(int argc, char* argv[]);
};
