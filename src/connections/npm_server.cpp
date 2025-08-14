#include "config.h"
#include "servers.h"

#include <format.h>
#include <poll.h>
#include <iostream>
#include <regex>
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

Message NPMServer::parse(const std::string& line) const {
  std::istringstream is{line};

  std::string cmd;
  is >> cmd;

  if (cmd == "npm:")
    return {NOTIFIER_ID, ""};

  return {NOTIFIER_ID, cmd};
}

NPMServer::NPMServer() noexcept
    : Endpoint{NPM_ID},
      p{{"npm", "start", "--", "--port", std::format("{}", config.port)},
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
      std::cerr << buf;
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
            send(parse(line));
          }
        }
      }

      msg_q.sleep_for(seconds{5});
    }
  }};

  std::cout << "[npm_server] started at port " << config.port << std::endl;
}

NPMServer::~NPMServer() noexcept {
  stop();
  if (t.joinable())
    t.join();

  p.kill();
  std::cout << "[exit] npm_server" << std::endl;
}

inline std::regex url_regex{R"(https://[a-zA-Z0-9\-]+\.trycloudflare\.com)"};

CloudflareServer::CloudflareServer() noexcept
    : Endpoint{CLDF_ID},
      p{{"cloudflared", "tunnel", "--url",
         std::format("http://localhost:{}", config.port)},
        output{PIPE},
        input{PIPE},
        error{PIPE}},
      failed{false}  //
{
  t = std::thread{[&]() {
    auto child_err = p.error();
    int fd = fileno(child_err);

    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(60);

    std::string buffer;
    char buf[256];

    while (!is_stopped()) {
      // Check if timeout exceeded
      auto elapsed = std::chrono::steady_clock::now() - start;
      if (elapsed > timeout) {
        std::cerr << "[cloudflare] timeout waiting for url\n";
        failed = true;
        p.kill();
        break;
      }

      // Poll with 100ms timeout
      struct pollfd pfd = {fd, POLLIN, 0};
      int ret = poll(&pfd, 1, 100);

      if (ret > 0 && (pfd.revents & POLLIN)) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
          buf[n] = '\0';
          buffer += buf;

          // Check for URL in accumulated buffer
          std::smatch match;
          if (std::regex_search(buffer.cbegin(), buffer.cend(), match,
                                url_regex)) {
            url = match[0];
            break;
          }
        } else if (n == 0) {
          std::cerr << "[cloudflare] process terminated unexpectedly\n";
          failed = true;
          break;
        }
      } else if (ret < 0 && errno != EINTR) {
        std::cerr << "[cloudflare] poll error: " << strerror(errno) << "\n";
        failed = true;
        p.kill();
        break;
      }
    }

    if (!failed && url != "") {
      std::cout << "[cloudflare] " << url << std::endl;
      send(NOTIFIER_ID, "tunnel_url", {{"url", url}});
    }
  }};
}

CloudflareServer::~CloudflareServer() noexcept {
  stop();
  if (t.joinable())
    t.join();

  if (failed) {
    std::cout << "[exit] cloudflare with error" << std::endl;
    return;
  }

  p.kill();
  std::cout << "[exit] cloudflare" << std::endl;
}
