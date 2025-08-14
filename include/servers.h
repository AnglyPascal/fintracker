#pragma once

#include "message.h"

#include <cpp-subprocess/subprocess.hpp>

#include <deque>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

class TGServer : public Endpoint {
  mutable std::set<int> seen_updates;
  int last_update_id = 0;

  std::thread t_receive, t_send;
  void t_receive_f();
  void t_send_f();

  Message parse(const std::string&) const;

 public:
  TGServer() noexcept;
  ~TGServer() noexcept;
};

namespace sp = subprocess;

class NPMServer : public Endpoint {
  sp::Popen p;
  std::thread t;

  Message parse(const std::string& line) const;

 public:
  NPMServer() noexcept;
  ~NPMServer() noexcept;
};

class CloudflareServer : public Endpoint {
 private:
  sp::Popen p;
  std::thread t;
  std::string url;
  bool failed;

 public:
  CloudflareServer() noexcept;
  ~CloudflareServer() noexcept;
};
