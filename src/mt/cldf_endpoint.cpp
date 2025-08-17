#include "mt/endpoints.h"
#include "util/config.h"

#include <poll.h>
#include <format>
#include <iostream>
#include <regex>
#include <thread>

#include <spdlog/spdlog.h>
#include <cpp-subprocess/subprocess.hpp>

inline std::regex url_regex{R"(https://[a-zA-Z0-9\-]+\.trycloudflare\.com)"};

using namespace subprocess;

CloudflareEndpoint::CloudflareEndpoint() noexcept
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
      send_to_broker(Message{id, PORTFOLIO_ID, "tunnel_url", {{"url", url}}});
    }
  }};
}

CloudflareEndpoint::~CloudflareEndpoint() noexcept {
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
