#pragma once

#include "concurrency/message.h"

#include <cpp-subprocess/subprocess.hpp>

#include <deque>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

class TGEndpoint : public Endpoint {
  mutable std::set<int> seen_updates;
  int last_update_id = 0;

  std::thread t_receive, t_send;
  void t_receive_f();
  void t_send_f();

  Message parse(const std::string&) const;

 public:
  TGEndpoint() noexcept;
  ~TGEndpoint() noexcept;
};

namespace sp = subprocess;

class NPMEndpoint : public Endpoint {
  sp::Popen p;
  std::thread t;

  Message parse(const std::string& line) const;

 public:
  NPMEndpoint() noexcept;
  ~NPMEndpoint() noexcept;
};

class CloudflareEndpoint : public Endpoint {
 private:
  sp::Popen p;
  std::thread t;
  std::string url;
  bool failed;

 public:
  CloudflareEndpoint() noexcept;
  ~CloudflareEndpoint() noexcept;
};

