#pragma once

#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <chrono>
#include <stdexcept>

struct SignalFD {
  int fd = -1;
  uint32_t signo;

  explicit SignalFD(uint32_t signo) : signo{signo} {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signo);

    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) == -1)
      throw std::runtime_error("sigprocmask failed");

    fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (fd == -1)
      throw std::runtime_error("signalfd failed");
  }

  SignalFD(const SignalFD&) = delete;
  SignalFD& operator=(const SignalFD&) = delete;
  SignalFD(SignalFD&& o) noexcept : fd{o.fd}, signo{o.signo} { o.fd = -1; }
  SignalFD& operator=(SignalFD&& o) noexcept {
    if (this != &o) {
      if (this->fd != o.fd && fd != -1)
        close(fd);
      fd = o.fd;
      signo = o.signo;
      o.fd = -1;
    }
    return *this;
  }

  ~SignalFD() {
    if (fd != -1)
      close(fd);
  }
};

inline bool sleep_with(SignalFD& sfd, std::chrono::milliseconds timeout) {
  struct pollfd pfd{sfd.fd, POLLIN, 0};

  int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
  if (ret == -1) {
    if (errno == EINTR)
      return true;  // pretend we timed out

    perror("poll");
    return false;
  }

  if (ret == 0)
    return true;

  if (pfd.revents & POLLIN) {
    signalfd_siginfo si;

    ssize_t n = read(sfd.fd, &si, sizeof(si));
    if (n != sizeof(si)) {
      perror("read(signalfd)");
      return false;
    }

    if (sfd.signo == 0 || si.ssi_signo == sfd.signo)
      return false;

    return true;
  }

  return false;
}
