#include "mt/endpoints.h"
#include "util/config.h"

#include <poll.h>
#include <format>
#include <iostream>
#include <thread>

#include <spdlog/spdlog.h>
#include <cpp-subprocess/subprocess.hpp>

using namespace subprocess;

inline auto split_first_newline(const std::string& str) {
  size_t pos = str.find('\n');
  if (pos == std::string::npos)
    return std::make_pair(str, std::string{});
  return std::make_pair(str.substr(0, pos), str.substr(pos + 1));
}

Message NPMEndpoint::parse(const std::string& line) const {
  std::istringstream is{line};
  std::string cmd;
  is >> cmd;
  return {id, PORTFOLIO_ID, cmd};
}

NPMEndpoint::NPMEndpoint() noexcept
    : Endpoint{NPM_ID},
      p{{"npm", "start", "--", "--port", std::format("{}", config.port),
         "--reload-port", std::format("{}", config.reload_port)},
        cwd{"./page"},
        output{PIPE},
        input{PIPE},
        error{PIPE}}  //
{
  {
    auto child_out = p.output();
    char buf[256];
    for (int i = 0; i < 5; i++) {
      fgets(buf, sizeof(buf), child_out);
      // std::cerr << buf;
    }
  }

  t = std::thread{[&] {
    auto child_out = p.output();
    auto fd = fileno(child_out);

    std::string buffer;
    char buf[256];

    while (!is_stopped()) {
      struct pollfd pfd = {fd, POLLIN, 0};
      int ret = poll(&pfd, 1, 100);

      if (ret > 0 && (pfd.revents & POLLIN)) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
          buf[n] = '\0';
          buffer += buf;

          while (!buffer.empty()) {
            auto [line, rest] = split_first_newline(buffer);
            buffer = rest;
            send_to_broker(parse(line));
          }
        }
      }

      msg_q.sleep_for(seconds{5});
    }
  }};

  std::cout << "[npm_server] started at port " << config.port << std::endl;
}

NPMEndpoint::~NPMEndpoint() noexcept {
  stop();
  if (t.joinable())
    t.join();

  p.kill();
  std::cout << "[exit] npm_server" << std::endl;
}
