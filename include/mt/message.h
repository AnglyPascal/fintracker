#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using id_t = uint32_t;
using kv_list_t =
    std::initializer_list<std::pair<const std::string, std::string>>;

inline id_t message_id = 0;

struct Message {
  const id_t id = 0;
  const id_t from;
  const id_t to;

  std::string cmd;
  std::unordered_map<std::string, std::string> params;

  Message(id_t id, id_t from, id_t to, std::string cmd)
      : id{id}, from{from}, to{to}, cmd{std::move(cmd)}, params{} {}
  Message(id_t id, id_t from, id_t to, std::string cmd, kv_list_t params)
      : id{id}, from{from}, to{to}, cmd{std::move(cmd)}, params{params} {}

  Message(id_t from, id_t to, std::string cmd)
      : id{message_id++}, from{from}, to{to}, cmd{std::move(cmd)}, params{} {}
  Message(id_t from, id_t to, std::string cmd, kv_list_t params)
      : id{message_id++},
        from{from},
        to{to},
        cmd{std::move(cmd)},
        params{params} {}
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
    if (is_stopped())
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

class MessageBroker;

class Endpoint {
 private:
  std::mutex broker_mtx;
  std::condition_variable cv;
  std::optional<std::reference_wrapper<MessageBroker>> broker;

 protected:
  const id_t id;

  MessageQueue msg_q;
  std::unordered_map<id_t, std::vector<Message>> unsent;

  Endpoint(id_t id) : id{id} {}
  bool is_stopped() const { return msg_q.is_stopped(); }

  void send_to_broker(Message&& msg);

  void send_to_broker(id_t msg_id, id_t to, std::string cmd) {
    return send_to_broker(Message{msg_id, id, to, cmd});
  }

  void send_to_broker(id_t msg_id, id_t to, std::string cmd, kv_list_t params) {
    return send_to_broker(Message{msg_id, id, to, cmd, params});
  }

  void send_to_broker(id_t to, std::string cmd) {
    return send_to_broker(Message{id, to, cmd});
  }

  void send_to_broker(id_t to, std::string cmd, kv_list_t params) {
    return send_to_broker(Message{id, to, cmd, params});
  }

  void set_broker(MessageBroker& b) {
    {
      std::lock_guard lk{broker_mtx};
      broker = std::ref(b);
    }
    cv.notify_all();
  }

  friend class MessageBroker;

 public:
  void send(Message&& msg) { msg_q.push(std::move(msg)); }

  void stop() {
    if (is_stopped())
      return;
    msg_q.stop();
  };
};

enum : id_t {
  NPM_ID = 0,
  TG_ID = 1,
  PORTFOLIO_ID = 2,
  CLDF_ID = 3,
};

class MessageBroker {
  std::unordered_map<id_t, std::reference_wrapper<Endpoint>> endpoints;
  std::unordered_map<id_t, std::vector<Message>> unsent;

  friend inline void connect(MessageBroker& broker, Endpoint& endpoint);

 public:
  template <typename... Args>
  void send(Args&&... args) {
    Message msg{std::forward<Args>(args)...};
    auto it = endpoints.find(msg.to);
    if (it == endpoints.end()) {
      unsent[msg.to].push_back(std::move(msg));
      return;
    }
    auto endpoint = it->second;
    endpoint.get().send(std::move(msg));
  }

  template <typename... Args>
    requires(std::derived_from<Args, Endpoint> && ...)
  MessageBroker(Args&... args) {
    bool stop = (  //
        [this](auto& args) {
          if (args.is_stopped())
            return true;
          endpoints.try_emplace(args.id, std::ref(args));
          return false;
        }(args) ||
        ...  //
    );

    if (stop)
      for (auto& [_, endpoint] : endpoints)
        endpoint.get().stop();

    (
        [this](auto& args) {
          if (args.is_stopped())
            return;
          args.set_broker(*this);
        }(args),
        ...  //
    );
  }
};

inline void Endpoint::send_to_broker(Message&& msg) {
  std::unique_lock lk{broker_mtx};
  cv.wait(lk, [&] { return broker.has_value(); });
  if (broker)
    broker->get().send(std::move(msg));
}
