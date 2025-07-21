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
