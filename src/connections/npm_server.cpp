#include "notifier.h"

NPMServer::NPMServer() noexcept {
  asio::io_context ctx;
  process ls(ctx.get_executor(), "/ls", {}, process_start_dir("/home"));
  ls.wait();
}


