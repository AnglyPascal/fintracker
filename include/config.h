#pragma once

#include <string>

struct Config {
  bool debug_en = false;

  bool tg_en = true;
  bool plot_en = true;

  bool replay_en = false;

  bool continuous_en = false;
  double speed = 1.0;

  Config(int argc, char* argv[]);
};

struct PositionSizingConfig {
  double capital = 4000.0;
  std::string capital_currency = "GBP";
  double capital_usd = -1.0;

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
