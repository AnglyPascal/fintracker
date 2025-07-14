#include "config.h"
#include "notifier.h"
#include "portfolio.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace fs = std::filesystem;

void init_logging() {
  auto pwd = fs::current_path().generic_string();
  auto log_name = std::format("{}/logs/{:%F_%R}.log", pwd, SysClock::now());
  auto link_name = pwd + "/logs/output.log";

  fs::remove(link_name);
  fs::create_symlink(log_name, link_name);

  auto file_logger = spdlog::basic_logger_mt("file_logger", log_name);
  spdlog::set_default_logger(file_logger);

  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
}

int main(int argc, char* argv[]) {
  init_logging();

  Config config{argc, argv};
  Portfolio portfolio{config};
  Notifier notifier{portfolio};
  portfolio.run();
  return 0;
}

