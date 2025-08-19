#pragma once

#include <string>
#include <vector>

struct SymbolInfo {
  char add;
  std::string symbol;
  int priority;
  std::string sector;
};

struct Symbols {
  std::vector<SymbolInfo> arr;

  Symbols() noexcept;

  auto size() const { return arr.size(); }

  auto operator[](int x) const { return arr[x]; }
  auto operator[](int x) { return arr[x]; }

  auto begin() { return arr.begin(); }
  auto begin() const { return arr.begin(); }

  auto end() { return arr.end(); }
  auto end() const { return arr.end(); }
};
