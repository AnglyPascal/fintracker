#pragma once

struct Config {
  bool tg_en = true;
  bool backtest_en = false;
  bool plot_en = true;
  bool debug_en = false;

  Config(int argc, char* argv[]);
};
