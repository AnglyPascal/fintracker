#include "config.h"

#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <boost/pfr.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <glaze/glaze.hpp>

namespace fs = std::filesystem;

template <typename T>
T read(const char* path) {
  T t{};

  auto ec = glz::read_file_json(t, path, std::string{});
  if (ec)
    std::cerr << glz::format_error(ec) << std::endl;

  if (T::debug) {
    std::ofstream log{"logs/configs.log", std::ios::app};
    std::string buffer;
    auto _ = glz::write<glz::opts{.prettify = true}>(t, buffer);
    log << std::format("\"{}\": {}\n", T::name, buffer.c_str());
  }

  return t;
}

Config::Config() {
  fs::remove("logs/configs.log");

  api_config = read<APIConfig>("private/api.json");
  ind_config = read<IndicatorsConfig>("private/indicators.json");
  sizing_config = read<PositionSizingConfig>("private/sizing.json");
  sr_config = read<SupportResistanceConfig>("private/support_resistance.json");
  sig_config = read<SignalConfig>("private/signal.json");
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
}
