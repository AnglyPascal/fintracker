#include "config.h"

#include <argparse/argparse.hpp>

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

  program.add_argument("-bt", "--backtest")
      .default_value(false)
      .implicit_value(true)
      .help("Enable backtesting mode");

  program.add_argument("-p", "--disable-plot")
      .default_value(false)
      .implicit_value(true)
      .help("Enable backtesting mode");

  program.add_argument("-c", "--continuous")
      .default_value(false)
      .implicit_value(true)
      .help("Enable continuous backtesting mode");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << "\n" << program << "\n";
  }

  tg_en = !program.get<bool>("--disable-tg");
  backtest_en = program.get<bool>("--backtest");
  plot_en = !program.get<bool>("--disable-plot");
  debug_en = program.get<bool>("--debug");
  continuous_en = program.get<bool>("--continuous");
}
