#include <cpr/cpr.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <vector>

using json = nlohmann::json;

struct Event {
  std::string type;
  std::string symbol;
  std::string date;
  std::string info;
};

std::string date_to_string(std::chrono::system_clock::time_point tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm = *std::localtime(&t);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d");
  return oss.str();
}

std::vector<Event> fetch_nasdaq_events(const std::string& event_type,
                                       const std::string& date) {
  std::vector<Event> events;
  auto url = std::format("https://api.nasdaq.com/api/calendar/{}?date={}",
                         event_type, date);

  cpr::Response r =
      cpr::Get(cpr::Url{url}, cpr::Header{{"User-Agent", "Mozilla/5.0"}});

  if (r.status_code != 200) {
    std::cerr << "Failed to fetch " << event_type << " for " << date
              << ": HTTP " << r.status_code << "\n";
    return events;
  }

  try {
    auto root = json::parse(r.text);
    auto data = root["data"];
    if (!data.is_object())
      return events;

    if (event_type == "earnings") {
      auto earnings = data["rows"];
      for (auto& item : earnings) {
        events.push_back(
            Event{"Earnings", item.value("symbol", ""), date,
                  "EPS estimate: " + item.value("epsForecast", "")});
      }
    } else if (event_type == "dividends") {
      auto dividends = data["rows"];
      for (auto& item : dividends) {
        events.push_back(
            Event{"Dividends", item.value("symbol", ""), date,
                  "Dividend rate: " + item.value("dividend_Rate", "")});
      }
    } else if (event_type == "splits") {
      auto splits = data["rows"];
      for (auto& item : splits) {
        events.push_back(Event{"Splits", item.value("symbol", ""), date,
                               "Ratio: " + item.value("ratio", "")});
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error for " << event_type << " on " << date << ": "
              << e.what() << "\n";
  }
  return events;
}

using SymbolInfo = std::pair<std::string, int>;

inline std::vector<SymbolInfo> read_symbols() {
  std::vector<SymbolInfo> symbols;
  std::ifstream file("data/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string add, symbol, tier_str;

    if (std::getline(ss, add, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, tier_str)) {
      if (add == "+")
        symbols.push_back({symbol, std::stoi(tier_str)});
    }
  }

  return symbols;
}

std::string to_html(const std::vector<Event>& input_events) {
  std::vector<Event> events = input_events;
  std::sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) { return a.date < b.date; });

  std::ostringstream html;
  html << "<!DOCTYPE html><html><head><meta charset='utf-8'>"
       << "<style>"
       << "table { border-collapse: collapse; width: 100%; }"
       << "th, td { border: 1px solid #ccc; padding: 6px; text-align: left; }"
       << "th { background-color: #f2f2f2; }"
       << "</style></head><body>"
       << "<h2>Upcoming Events</h2>"
       << "<table><tr><th>Date</th><th>Symbol</th><th>Type</th><th>Info</th></"
          "tr>";

  for (const auto& ev : events) {
    std::string row =
        std::format("<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>\n",
                    ev.date, ev.symbol, ev.type, ev.info);
    html << row;
  }

  html << "</table></body></html>";
  return html.str();
}

int main() {
  std::unordered_set<std::string> watchlist;
  for (auto& [symbol, _] : read_symbols())
    watchlist.insert(symbol);

  std::vector<Event> filtered_events;

  auto today = std::chrono::system_clock::now();

  for (int i = 0; i < 30; ++i) {
    auto day = today + std::chrono::hours(24 * i);
    std::string date = date_to_string(day);

    for (const auto& event_type : {"earnings", "dividends", "splits"}) {
      auto events = fetch_nasdaq_events(event_type, date);
      for (const auto& ev : events) {
        if (watchlist.count(ev.symbol) > 0)
          filtered_events.push_back(ev);
      }
    }
  }

  {
    std::ofstream f("page/calendar.html");
    f << to_html(filtered_events);
    f.close();
  }

  {
    std::ofstream f("data/calendar.csv");
    f << "type,symbol,date,info" << std::endl;
    for (auto& [type, symbol, date, info] : filtered_events)
      f << std::format("{},{},{}\n", type, symbol, date);
    f.close();
  }
}
