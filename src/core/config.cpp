#include "util/config.h"

#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fstream>
#include <glaze/glaze.hpp>
#include <thread>

namespace fs = std::filesystem;

template <typename T>
T read(const char* path) {
  T t{};

  auto ec = glz::read_file_json(t, path, std::string{});
  if (ec)
    std::cerr << std::format("[config] {} error {}\n", path,
                             glz::format_error(ec));

  if (T::debug) {
    std::ofstream log{"logs/configs.log", std::ios::app};
    std::string buffer;
    auto _ = glz::write<glz::opts{.prettify = true}>(t, buffer);
    log << std::format("\"{}\": {}\n", T::name, buffer.c_str());
  }

  return t;
}

Config::Config() {
  update();
}

double to_usd(double amount, const std::string& currency = "GBP") noexcept;

void Config::update() {
  fs::remove("logs/configs.log");

  api_config = read<APIConfig>("private/api.json");
  ind_config = read<IndicatorsConfig>("private/indicators.json");
  sizing_config = read<PositionSizingConfig>("private/sizing.json");
  sr_config = read<SupportResistanceConfig>("private/support_resistance.json");
  sig_config = read<SignalConfig>("private/signal.json");

  sizing_config.capital_usd =
      to_usd(sizing_config.capital, sizing_config.capital_currency);
}

void Config::read_args(int argc, char* argv[]) {
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

  program.add_argument("-R", "--replay-paused")
      .default_value(false)
      .implicit_value(true)
      .help("Enable replaying mode");

  program.add_argument("-c", "--clear")
      .default_value(false)
      .implicit_value(true)
      .help("Clear previous replay data, fetch anew");

  program.add_argument("-l", "--disable-plot")
      .default_value(false)
      .implicit_value(true)
      .help("Disable plot generation");

  program.add_argument("-s", "--speed")
      .help("Speed of continuous replay")
      .default_value(0.0)
      .scan<'g', double>();

  program.add_argument("--port")
      .help("Port for the webpage")
      .default_value(8000)
      .scan<'d', int>();

  auto def_nthreads = static_cast<size_t>(std::thread::hardware_concurrency());
  program.add_argument("--nthreads")
      .help("Max number of concurrent threads")
      .default_value(def_nthreads)
      .scan<'d', size_t>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << "\n" << program << "\n";
  }

  tg_en = !program.get<bool>("--disable-tg");
  replay_clear = program.get<bool>("--clear");
  plot_en = !program.get<bool>("--disable-plot");
  debug_en = program.get<bool>("--debug");
  speed = program.get<double>("--speed");
  port = program.get<int>("--port");

  replay_en =
      program.get<bool>("--replay") || program.get<bool>("--replay-paused");
  replay_paused = program.get<bool>("--replay-paused");

  n_concurrency = program.get<size_t>("--nthreads");
}
