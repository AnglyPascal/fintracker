#pragma once

#include "sleeper.h"

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <latch>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

template <typename T>
  requires std::is_move_assignable_v<T> && std::is_move_constructible_v<T>
class thread_pool {
  const size_t n_threads = 1;
  std::vector<std::jthread> threads;
  std::latch latch;

  using Func = std::function<bool(T&&)>;
  const Func func;

  std::vector<T> vals;
  mutable std::mutex mtx;
  std::condition_variable cv;
  bool stopped = false;
  bool started = false;

  std::optional<T> pop() {
    std::unique_lock lk{mtx};
    cv.wait(lk, [this] { return stopped || (!vals.empty() && started); });
    if (vals.empty())
      return std::nullopt;
    auto t = std::move(vals.back());
    vals.pop_back();
    return t;
  }

  void worker_loop() {
    while (!sleeper.should_shutdown()) {
      auto t_opt = pop();
      if (!t_opt)
        break;
      auto cont = func(std::move(*t_opt));
      if (!cont) {
        stop();
        break;
      }
    }
    latch.count_down();
  }

  void stop() {
    {
      std::lock_guard lk{mtx};
      stopped = true;
    }
    cv.notify_all();
  }

 public:
  thread_pool(size_t n_threads, Func func, std::vector<T> vec) noexcept
      : n_threads{n_threads},
        latch{static_cast<ptrdiff_t>(n_threads)},
        func{func},
        vals{std::move(vec)}  //
  {
    threads.reserve(n_threads);
    for (size_t i = 0; i < n_threads; i++)
      threads.emplace_back(&thread_pool::worker_loop, this).detach();
    {
      std::lock_guard lk{mtx};
      started = true;
    }
    cv.notify_all();
  }

  ~thread_pool() {
    stop();
    latch.wait();
  }

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;

  template <typename... Args>
    requires std::constructible_from<T, Args...>
  void emplace(Args&&... args) {
    {
      std::lock_guard lk{mtx};
      if (stopped)
        throw std::runtime_error("added work to stopped thread_pool");
      vals.emplace_back(std::forward<Args>(args)...);
    }
    cv.notify_one();
  }
};
