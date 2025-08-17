#pragma once

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>

class Sleeper {
  std::atomic<bool> shutdown_requested{false};
  std::mutex mtx;
  std::condition_variable cv;
  std::thread td;

  inline void handler() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    int signum;
    while (true) {
      if (sigwait(&set, &signum) == 0) {
        if (signum == SIGINT) {
          std::cout << "\b \b" << "\b \b" << std::flush;
          request_shutdown();
          break;
        }
      }
    }
  }

  void block_signals_for_all_threads() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0)
      throw std::runtime_error("Failed to block signals");
  }

 public:
  Sleeper() {
    block_signals_for_all_threads();
    td = std::thread(&Sleeper::handler, this);
  }

  ~Sleeper() {
    if (!should_shutdown())
      request_shutdown();

    if (td.joinable()) {
      // Send SIGINT to ourselves to wake up the signal thread for clean
      // shutdown
      pthread_kill(td.native_handle(), SIGINT);
      td.join();
    }
  }

  Sleeper(const Sleeper&) = delete;
  Sleeper& operator=(const Sleeper&) = delete;
  Sleeper(Sleeper&&) = delete;
  Sleeper& operator=(Sleeper&&) = delete;

  bool should_shutdown() const {
    return shutdown_requested.load(std::memory_order_acquire);
  }

  void request_shutdown() {
    shutdown_requested.store(true, std::memory_order_release);
    cv.notify_all();
  }

  template <typename Rep, typename Period>
  bool sleep_for(const std::chrono::duration<Rep, Period> duration) {
    if (should_shutdown())
      return false;
    std::unique_lock lk{mtx};
    return !cv.wait_for(lk, duration, [this] { return should_shutdown(); });
  }

  void wait_for_shutdown() {
    std::unique_lock lk{mtx};
    cv.wait(lk, [this] { return should_shutdown(); });
  }
};

inline Sleeper sleeper;
