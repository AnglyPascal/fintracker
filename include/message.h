#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using id_t = uint32_t;
using kv_list_t =
    std::initializer_list<std::pair<const std::string, std::string>>;

struct Message {
  const id_t id;
  std::string cmd;
  std::unordered_map<std::string, std::string> params;

  Message(id_t id, std::string cmd) : id{id}, cmd{std::move(cmd)}, params{} {}
  Message(id_t id, std::string cmd, kv_list_t params)
      : id{id}, cmd{std::move(cmd)}, params{params} {}
};

class MessageQueue {
 private:
  std::deque<Message> msgs;
  std::condition_variable cv;
  mutable std::mutex mtx;
  bool stopped = false;

 public:
  template <typename... Args>
  void push(Args&&... args) {
    std::lock_guard lk{mtx};
    msgs.emplace_back(std::forward<Args>(args)...);
    cv.notify_all();
  }

  std::optional<Message> pop() {
    std::unique_lock lk{mtx};
    cv.wait(lk, [this] { return stopped || !msgs.empty(); });
    if (msgs.empty())
      return std::nullopt;
    auto msg = std::move(msgs.front());
    msgs.pop_front();
    return msg;
  }

  void stop() {
    if (stopped)
      return;
    std::lock_guard lk{mtx};
    stopped = true;
    cv.notify_all();
  }

  bool is_stopped() const { return stopped; }

  template <typename... Args>
  void sleep_for(const std::chrono::duration<Args...>& dur) {
    std::unique_lock lk{mtx};
    cv.wait_for(lk, dur, [&] { return stopped; });
  }
};

class Endpoint {
 protected:
  const id_t id;
  std::vector<std::reference_wrapper<Endpoint>> targets;
  MessageQueue msg_q;
  std::unordered_map<id_t, std::vector<Message>> unsent;

  template <typename... Args>
    requires(std::same_as<Args, Endpoint> && ...)
  Endpoint(id_t id, Args&... args) : id{id}, targets{std::ref(args)...} {}

  template <typename... Args>
  void receive(Args&&... args) {
    msg_q.push(std::forward<Args>(args)...);
  }

  void send(Message&& msg) {
    for (auto& target : targets) {
      if (target.get().id == msg.id) {
        target.get().receive(std::move(msg));
        return;
      }
    }
    unsent[msg.id].emplace_back(std::move(msg));
  }

  void send(id_t id, const std::string& cmd, kv_list_t kvs) {
    send(Message{id, cmd, kvs});
  }

  bool is_stopped() const { return msg_q.is_stopped(); }

 public:
  void add_target(Endpoint& ep) {
    targets.push_back(std::ref(ep));
    if (auto it = unsent.find(ep.id); it != unsent.end()) {
      auto vec = std::move(it->second);
      unsent.erase(it);
      for (auto& msg : vec)
        ep.receive(std::move(msg));
    }
  }

  void stop() {
    if (is_stopped())
      return;
    msg_q.stop();
    for (auto target : targets)
      target.get().stop();
  };
};

inline constexpr id_t NPM_ID = 0;
inline constexpr id_t TG_ID = 1;
inline constexpr id_t NOTIFIER_ID = 2;
inline constexpr id_t CLDF_ID = 3;

