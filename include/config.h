#pragma once

struct Config {
  bool debug_en = false;

  bool tg_en = true;
  bool plot_en = true;

  bool replay_en = false;

  bool continuous_en = false;
  double speed = 1.0;

  Config(int argc, char* argv[]);
};
