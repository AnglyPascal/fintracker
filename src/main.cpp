#include "config.h"
#include "notifier.h"
#include "portfolio.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>

namespace fs = std::filesystem;

inline void init_logging() {
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

inline void ensure_directories_exist(const std::vector<std::string>& dirs) {
  for (const auto& dir : dirs) {
    fs::path path{dir};
    if (fs::exists(path))
      continue;
    if (fs::create_directories(path))
      std::cout << "Created: " << dir << '\n';
    else
      std::cerr << "Failed to create: " << dir << '\n';
  }
}

struct SignalFD {
  int fd{-1};

  explicit SignalFD(int signo) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signo);

    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1)
      throw std::runtime_error("sigprocmask failed");

    fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (fd == -1)
      throw std::runtime_error("signalfd failed");
  }

  ~SignalFD() {
    if (fd != -1)
      close(fd);
  }
};

inline bool wait_with_sigint(int sfd, std::chrono::milliseconds timeout) {
  struct pollfd fds;
  fds.fd = sfd;
  fds.events = POLLIN;

  int ret = poll(&fds, 1, static_cast<int>(timeout.count()));

  if (ret == -1) {
    perror("poll");
    return false;
  }

  if (ret == 0)
    return true;

  if (fds.revents & POLLIN) {
    struct signalfd_siginfo si;
    ssize_t res = read(sfd, &si, sizeof(si));
    if (res != sizeof(si)) {
      perror("read");
    } else if (si.ssi_signo == SIGINT) {
      std::cerr << "SIGINT received\n";
      return false;
    }
  }

  return true;
}

int main(int argc, char* argv[]) {
  ensure_directories_exist({"page", "page/src", "page/backup", "logs", "data"});
  config.read_args(argc, argv);
  init_logging();

  {
    TGServer tg;
    NPMServer npm;
    CloudflareServer cfl;

    Portfolio portfolio{};
    Notifier notifier{portfolio};

    notifier.add_target(tg);
    tg.add_target(notifier);

    notifier.add_target(npm);
    npm.add_target(notifier);

    notifier.add_target(cfl);
    cfl.add_target(notifier);

    // FIXME: what happens when replay is enabled
    SignalFD sfd{SIGINT};
    portfolio.run(
        [sfd](auto millis) { return wait_with_sigint(sfd.fd, millis); });
  }

  std::cout << "[exit] main" << std::endl;
  return 0;
}
