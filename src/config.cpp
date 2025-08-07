#include "config.h"

#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

Config::Config(int argc, char* argv[]) {
  argparse::ArgumentParser program("fin");

  program.add_argument("-d", "--debug")
      .default_value(false)
      .implicit_value(true)
      .help("Enable debug");

  program.add_argument("-t", "--disable-tg")
      .default_value(false)
      .implicit_value(true)
      .help("Disable Telegram");

  program.add_argument("-r", "--replay")
      .default_value(false)
      .implicit_value(true)
      .help("Enable replaying mode");

  program.add_argument("-p", "--disable-plot")
      .default_value(false)
      .implicit_value(true)
      .help("Enable replaying mode");

  program.add_argument("-c", "--continuous")
      .default_value(false)
      .implicit_value(true)
      .help("Enable continuous replaying mode");

  program.add_argument("-s", "--speed")
      .help("Speed of continuous replay")
      .default_value(1.0)
      .scan<'g', double>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << "\n" << program << "\n";
  }

  tg_en = !program.get<bool>("--disable-tg");
  replay_en = program.get<bool>("--replay");
  plot_en = !program.get<bool>("--disable-plot");
  debug_en = program.get<bool>("--debug");
  continuous_en = program.get<bool>("--continuous");
  speed = program.get<double>("--speed");

  sizing_config = PositionSizingConfig{"private/sizing.json"};
  sr_config = SupportResistanceConfig{"private/support_resistance.json"};
}

#define GET_FROM_JSON(name)                                           \
  do {                                                                \
    try {                                                             \
      if (j.contains(#name))                                          \
        j.at(#name).get_to(name);                                     \
    } catch (const std::exception& e) {                               \
      spdlog::warn("[pos] failed to read '{}': {}", #name, e.what()); \
    }                                                                 \
  } while (0)

PositionSizingConfig::PositionSizingConfig(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    spdlog::warn("[pos] failed to open positing sizing config file {}",
                 path.c_str());
    return;
  }

  try {
    nlohmann::json j;
    file >> j;

    GET_FROM_JSON(capital);
    GET_FROM_JSON(capital_currency);
    GET_FROM_JSON(max_capital_per_position);
    GET_FROM_JSON(max_risk_pct);

    GET_FROM_JSON(stop_pct);
    GET_FROM_JSON(stop_atr_multiplier);

    GET_FROM_JSON(trailing_trigger_atr);
    GET_FROM_JSON(trailing_atr_multiplier);
    GET_FROM_JSON(trailing_stop_pct);

    GET_FROM_JSON(swing_low_window);
    GET_FROM_JSON(swing_low_buffer);
    GET_FROM_JSON(ema_stop_pct);

    GET_FROM_JSON(entry_score_cutoff);
    GET_FROM_JSON(entry_risk_cutoff);
    GET_FROM_JSON(entry_confidence_cutoff);

    GET_FROM_JSON(min_hold_days);
    GET_FROM_JSON(max_hold_days);

    GET_FROM_JSON(earnings_volatility_buffer);
  } catch (const std::exception& e) {
    spdlog::error("[pos] error parsing positing sizing config file {}: {}",
                  path.c_str(), e.what());
  }
}

SupportResistanceConfig::SupportResistanceConfig(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    spdlog::warn("[pos] failed to open positing sizing config file {}",
                 path.c_str());
    return;
  }

  try {
    nlohmann::json j;
    file >> j;

    GET_FROM_JSON(lookback_days);
    GET_FROM_JSON(n_zones);
    GET_FROM_JSON(min_zone_confidence);

    GET_FROM_JSON(swing_window_1h);
    GET_FROM_JSON(zone_width_1h);
    GET_FROM_JSON(n_candles_in_zone_1h);

    GET_FROM_JSON(swing_window_4h);
    GET_FROM_JSON(zone_width_4h);
    GET_FROM_JSON(n_candles_in_zone_4h);

    GET_FROM_JSON(swing_window_1d);
    GET_FROM_JSON(zone_width_1d);
    GET_FROM_JSON(n_candles_in_zone_1d);
  } catch (const std::exception& e) {
    spdlog::error("[pos] error parsing positing sizing config file {}: {}",
                  path.c_str(), e.what());
  }
}
