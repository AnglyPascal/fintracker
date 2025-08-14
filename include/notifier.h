#pragma once

#include "message.h"
#include "servers.h"
#include "times.h"

#include <deque>
#include <string>
#include <thread>
#include <unordered_map>

struct Portfolio;
struct Signal;

enum class Commands {
  BUY,
  SELL,
  STATUS,
  PING,
  TRADES,
  POSITIONS,
};

class Notifier : public Endpoint {
  const Portfolio& portfolio;

  mutable LocalTimePoint last_updated;
  std::unordered_map<std::string, Signal> prev_signals;
  std::string tunnel_url;

  std::thread td;
  void iter();
  void handle_command(const Message& msg);

 public:
  Notifier(const Portfolio& portfolio);
  ~Notifier();
};

