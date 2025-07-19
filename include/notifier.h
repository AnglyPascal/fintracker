#pragma once
#include "api.h"
#include "times.h"

#include <string>
#include <thread>
#include <unordered_map>

struct Portfolio;
struct Signal;

class Notifier {
  const Portfolio& portfolio;
  const TG& tg;

  mutable LocalTimePoint last_updated;
  std::unordered_map<std::string, Signal> prev_signals;

  std::thread td;
  static void iter(Notifier* notifier);

 public:
  Notifier(const Portfolio& portfolio) noexcept;
  ~Notifier() noexcept;
};

