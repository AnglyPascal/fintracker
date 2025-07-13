#pragma once

#include <termios.h>
#include <unistd.h>
#include <cstdio>

class RawMode {
 public:
  RawMode() {
    tcgetattr(STDIN_FILENO, &orig_);
    termios raw = orig_;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  }

  ~RawMode() { tcsetattr(STDIN_FILENO, TCSANOW, &orig_); }

 private:
  termios orig_;
};
