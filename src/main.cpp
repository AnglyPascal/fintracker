#include "config.h"
#include "notifier.h"
#include "portfolio.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>

void init_logging() {
  auto file_logger = spdlog::basic_logger_mt(
      "file_logger", "logs/" + std::format("{:%F_%T}", SysClock::now()) + ".log");
  spdlog::set_default_logger(file_logger);

  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
}

int main(int argc, char* argv[]) {
  init_logging();

  Config config{argc, argv};
  Portfolio portfolio{config};
  Notifier notifier{portfolio};
  portfolio.run();
  return 0;
}

