#include "config.h"
#include "notifier.h"
#include "portfolio.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void init_logging(const Config& config) {
  auto pwd = fs::current_path().generic_string();
  auto log_name = std::format("{}/logs/{:%F_%R}.log", pwd, SysClock::now());
  auto link_name = pwd + "/logs/output.log";

  fs::remove(link_name);
  fs::create_symlink(log_name, link_name);

  auto file_logger = spdlog::basic_logger_mt("file_logger", log_name);
  spdlog::set_default_logger(file_logger);

  auto level = config.debug_en ? spdlog::level::debug : spdlog::level::info;
  spdlog::set_level(level);
  spdlog::flush_on(level);

  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
}

void ensure_directories_exist(const std::vector<std::string>& dirs) {
  for (const auto& dir : dirs) {
    fs::path path(dir);
    if (!fs::exists(path)) {
      if (fs::create_directories(path))
        std::cout << "Created: " << dir << '\n';
      else
        std::cerr << "Failed to create: " << dir << '\n';
    }
  }
}

int main(int argc, char* argv[]) {
  ensure_directories_exist({"page", "page/src", "page/backup", "logs", "data"});

  Config config{argc, argv};
  init_logging(config);

  if (config.replay_en)
    plot_daemon_port = "5556";

  Portfolio portfolio{config};
  Notifier notifier{portfolio};
  portfolio.run();
  return 0;
}

