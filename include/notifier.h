#pragma once
#include "times.h"

#include <string>
#include <thread>
#include <unordered_map>

struct Portfolio;
struct Signal;

class Notifier {
  const Portfolio& portfolio;
  mutable TimePoint last_updated;
  std::unordered_map<std::string, Signal> prev_signals;

  std::thread td;
  static void iter(Notifier* notifier);
  std::string init_update() const;

 public:
  Notifier(const Portfolio& portfolio) noexcept;
  ~Notifier() noexcept;
};

