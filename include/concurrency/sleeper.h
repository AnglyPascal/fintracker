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

  static Sleeper* instance;
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
          shutdown_requested.store(true, std::memory_order_release);
          cv.notify_all();
          break;
        }
      }
    }
  }

  void block_signals_for_all_threads() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    // Block these signals for all threads
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0) {
      throw std::runtime_error("Failed to block signals");
    }
  }

 public:
  Sleeper() {
    instance = this;
    block_signals_for_all_threads();
    td = std::thread(&Sleeper::handler, this);
  }

  ~Sleeper() {
    if (!shutdown_requested.load()) {
      shutdown_requested.store(true, std::memory_order_release);
      cv.notify_all();
    }

    if (td.joinable()) {
      // Send SIGINT to ourselves to wake up the signal thread for clean
      // shutdown
      pthread_kill(td.native_handle(), SIGINT);
      td.join();
    }

    instance = nullptr;
  }

  Sleeper(const Sleeper&) = delete;
  Sleeper& operator=(const Sleeper&) = delete;
  Sleeper(Sleeper&&) = delete;
  Sleeper& operator=(Sleeper&&) = delete;

  bool should_shutdown() const {
    return shutdown_requested.load(std::memory_order_acquire);
  }

  template <typename Rep, typename Period>
  bool sleep_for(const std::chrono::duration<Rep, Period> duration) {
    std::unique_lock lk{mtx};
    return cv.wait_for(lk, duration, [this] { return should_shutdown(); });
  }

  void wait_for_shutdown() {
    std::unique_lock lk{mtx};
    cv.wait(lk, [this] { return should_shutdown(); });
  }
};

inline Sleeper* Sleeper::instance = nullptr;
inline Sleeper sleeper;

