#include "notifier.h"
#include "portfolio.h"

int main() {
  Portfolio portfolio;
  Notifier notifier{portfolio};
  portfolio.run();
  return 0;
}

