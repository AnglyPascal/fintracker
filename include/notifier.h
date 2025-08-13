#pragma once

#include "times.h"

#include <string>
#include <thread>
#include <unordered_map>

struct Portfolio;
struct Signal;
struct TG;

enum class Commands {
  BUY,
  SELL,
  STATUS,
  PING,
  TRADES,
  POSITIONS,
};

class Notifier {
  const Portfolio& portfolio;
  const TG& tg;

  std::string tunnel_url;

  mutable LocalTimePoint last_updated;
  std::unordered_map<std::string, Signal> prev_signals;

  std::thread td;
  static void iter(Notifier* notifier);

  template <Commands command>
  void handle_command(std::istream& is);
  void handle_command(const std::string& line);

 public:
  Notifier(const Portfolio& portfolio);
  ~Notifier();
};

class NPMServer {
  NPMServer() noexcept;
  ~NPMServer() noexcept;
};
