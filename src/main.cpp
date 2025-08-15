#include "config.h"
#include "portfolio.h"
#include "servers.h"

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
  int fd = -1;
  uint32_t signo;

  explicit SignalFD(uint32_t signo) : signo{signo} {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signo);

    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) == -1)
      throw std::runtime_error("sigprocmask failed");

    fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (fd == -1)
      throw std::runtime_error("signalfd failed");
  }

  SignalFD(const SignalFD&) = delete;
  SignalFD& operator=(const SignalFD&) = delete;
  SignalFD(SignalFD&& o) noexcept : fd{o.fd}, signo{o.signo} { o.fd = -1; }
  SignalFD& operator=(SignalFD&& o) noexcept {
    if (this != &o) {
      if (this->fd != o.fd && fd != -1)
        close(fd);
      fd = o.fd;
      signo = o.signo;
      o.fd = -1;
    }
    return *this;
  }

  ~SignalFD() {
    if (fd != -1)
      close(fd);
  }
};

inline bool sleep_with(SignalFD& sfd, std::chrono::milliseconds timeout) {
  struct pollfd pfd{sfd.fd, POLLIN, 0};

  int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
  if (ret == -1) {
    if (errno == EINTR)
      return true;  // pretend we timed out

    perror("poll");
    return false;
  }

  if (ret == 0)
    return true;

  if (pfd.revents & POLLIN) {
    signalfd_siginfo si;

    ssize_t n = read(sfd.fd, &si, sizeof(si));
    if (n != sizeof(si)) {
      perror("read(signalfd)");
      return false;
    }

    if (sfd.signo == 0 || si.ssi_signo == sfd.signo)
      return false;

    return true;
  }

  return false;
}

int main(int argc, char* argv[]) {
  ensure_directories_exist({"page", "page/src", "page/backup", "logs", "data"});
  config.read_args(argc, argv);
  init_logging();

  SignalFD sfd{SIGINT};
  {
    TGServer tg;
    NPMServer npm;
    CloudflareServer cfl;
    Portfolio portfolio;
    MessageBroker broker{tg, npm, cfl, portfolio};

    portfolio.run([&sfd](auto millis) { return sleep_with(sfd, millis); });
  }

  std::cout << "[exit] main" << std::endl;
  return 0;
}
