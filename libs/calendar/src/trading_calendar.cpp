#include "bazaartalks/calendar/trading_calendar.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace bazaartalks::calendar {

namespace {

using date::day;
using date::month;
using date::sys_days;
using date::year;
using date::year_month_day;

sys_days days(year_month_day ymd) { return sys_days{ymd}; }

// date::year_month_day + days offset, via sys_days round-trip.
year_month_day add_days(year_month_day ymd, int n) {
  return year_month_day{days(ymd) + date::days{n}};
}

// --- Easter (Anonymous Gregorian algorithm) — same arithmetic as
// market_holidays.py's _easter(). -----------------------------------------

year_month_day easter_impl(int y) {
  int a = y % 19;
  int b = y / 100;
  int c = y % 100;
  int d = b / 4;
  int e = b % 4;
  int f = (b + 8) / 25;
  int g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30;
  int i = c / 4;
  int k = c % 4;
  int l = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * l) / 451;
  int mo = (h + l - 7 * m + 114) / 31;
  int da = ((h + l - 7 * m + 114) % 31) + 1;
  return year{y} / month{static_cast<unsigned>(mo)} / day{static_cast<unsigned>(da)};
}

// --- Fixed (recurring, same-date-every-year) holidays per market — ports
// _fixed() 1:1. --------------------------------------------------------

std::set<year_month_day> fixed_holidays(Market market, int y) {
  std::set<year_month_day> s;
  year_month_day ny = year{y} / month{1} / day{1};
  year_month_day xmas = year{y} / month{12} / day{25};
  switch (market) {
    case Market::US:
      s.insert(ny);
      s.insert(year{y} / month{7} / day{4});
      s.insert(xmas);
      break;
    case Market::India:
      s.insert(ny);
      s.insert(year{y} / month{1} / day{26});
      s.insert(year{y} / month{8} / day{15});
      s.insert(year{y} / month{10} / day{2});
      s.insert(xmas);
      break;
    case Market::Japan:
      s.insert(ny);
      s.insert(year{y} / month{1} / day{2});
      s.insert(year{y} / month{1} / day{3});
      s.insert(year{y} / month{2} / day{11});
      s.insert(year{y} / month{4} / day{29});
      s.insert(year{y} / month{5} / day{3});
      s.insert(year{y} / month{5} / day{4});
      s.insert(year{y} / month{5} / day{5});
      s.insert(year{y} / month{11} / day{3});
      s.insert(year{y} / month{11} / day{23});
      s.insert(year{y} / month{12} / day{31});
      break;
    case Market::Korea:
      s.insert(ny);
      s.insert(year{y} / month{3} / day{1});
      s.insert(year{y} / month{5} / day{5});
      s.insert(year{y} / month{6} / day{6});
      s.insert(year{y} / month{8} / day{15});
      s.insert(year{y} / month{10} / day{3});
      s.insert(year{y} / month{10} / day{9});
      s.insert(xmas);
      s.insert(year{y} / month{12} / day{31});
      break;
    case Market::Europe: {
      year_month_day e = easter_impl(y);
      s.insert(ny);
      s.insert(add_days(e, -2));  // Good Friday
      s.insert(add_days(e, 1));   // Easter Monday
      s.insert(year{y} / month{5} / day{1});
      s.insert(xmas);
      s.insert(year{y} / month{12} / day{26});
      s.insert(year{y} / month{12} / day{31});
      break;
    }
  }
  return s;
}

// --- Curated variable/regional holidays (lunar, US floating, Diwali,
// Golden Week gaps, ...) — verbatim transcription of Python's HOLIDAYS dict,
// keyed by (market, year) -> list of ISO dates. Any change to the Python
// source must be mirrored here (golden tests diff against the Python
// original, so drift here is caught, not silent). ------------------------

year_month_day parse_iso(std::string_view iso) {
  // "YYYY-MM-DD"
  int y = std::stoi(std::string(iso.substr(0, 4)));
  unsigned mo = static_cast<unsigned>(std::stoi(std::string(iso.substr(5, 2))));
  unsigned da = static_cast<unsigned>(std::stoi(std::string(iso.substr(8, 2))));
  return year{y} / month{mo} / day{da};
}

const std::map<Market, std::map<int, std::vector<std::string_view>>>& curated_holidays() {
  static const std::map<Market, std::map<int, std::vector<std::string_view>>> table = {
      {Market::US,
       {
           {2024, {"2024-01-15", "2024-02-19", "2024-05-27", "2024-06-19", "2024-09-02", "2024-11-28"}},
           {2025, {"2025-01-20", "2025-02-17", "2025-05-26", "2025-06-19", "2025-09-01", "2025-11-27"}},
           {2026, {"2026-01-19", "2026-02-16", "2026-05-25", "2026-06-19", "2026-09-07", "2026-11-26"}},
       }},
      {Market::India,
       {
           {2025, {"2025-03-14", "2025-03-31", "2025-04-14", "2025-08-27", "2025-10-21", "2025-11-05"}},
           {2026, {"2026-03-04", "2026-03-21", "2026-04-14", "2026-08-26", "2026-11-09"}},
       }},
      {Market::Japan,
       {
           {2025, {"2025-01-13", "2025-03-20", "2025-07-21", "2025-09-15", "2025-09-23", "2025-10-13"}},
           {2026,
            {"2026-01-12", "2026-03-20", "2026-07-20", "2026-09-21", "2026-09-22", "2026-09-23",
             "2026-10-12"}},
       }},
      {Market::Korea,
       {
           {2025, {"2025-01-28", "2025-01-29", "2025-01-30", "2025-10-06", "2025-10-07", "2025-10-08"}},
           {2026, {"2026-02-16", "2026-02-17", "2026-02-18", "2026-09-24", "2026-09-25"}},
       }},
      {Market::Europe, {}},  // fixed+Easter covers most XETRA/Euronext closures
  };
  return table;
}

std::set<year_month_day> holiday_set(Market market, int y) {
  std::set<year_month_day> s = fixed_holidays(market, y);
  auto mkt_it = curated_holidays().find(market);
  if (mkt_it != curated_holidays().end()) {
    auto yr_it = mkt_it->second.find(y);
    if (yr_it != mkt_it->second.end()) {
      for (auto iso : yr_it->second) s.insert(parse_iso(iso));
    }
  }
  return s;
}

}  // namespace

Market market_from_alias(std::string_view m) {
  static const std::unordered_map<std::string_view, Market> canon = {
      {"US", Market::US},         {"NYSE", Market::US},      {"NASDAQ", Market::US},
      {"India", Market::India},   {"NSE", Market::India},    {"BSE", Market::India},
      {"Japan", Market::Japan},   {"TSE", Market::Japan},    {"JPX", Market::Japan},
      {"Korea", Market::Korea},   {"KRX", Market::Korea},    {"KOSPI", Market::Korea},
      {"Europe", Market::Europe}, {"EU", Market::Europe},    {"XETRA", Market::Europe},
  };
  auto it = canon.find(m);
  if (it == canon.end()) {
    throw std::invalid_argument("bazaartalks::calendar: unknown market/alias '" +
                                 std::string(m) + "'");
  }
  return it->second;
}

date::year_month_day easter(int year) { return easter_impl(year); }

bool is_trading_day(Market market, year_month_day d) {
  sys_days sd = days(d);
  date::weekday wd{sd};
  if (wd == date::Saturday || wd == date::Sunday) return false;
  int y = static_cast<int>(d.year());
  auto holidays = holiday_set(market, y);
  return holidays.find(d) == holidays.end();
}

std::vector<year_month_day> trading_days(Market market, year_month_day start,
                                          year_month_day end) {
  std::vector<year_month_day> out;
  sys_days cur = days(start);
  sys_days last = days(end);
  for (; cur <= last; cur += date::days{1}) {
    year_month_day ymd{cur};
    date::weekday wd{cur};
    if (wd == date::Saturday || wd == date::Sunday) continue;
    if (is_trading_day(market, ymd)) out.push_back(ymd);
  }
  return out;
}

year_month_day next_trading_day(Market market, year_month_day d) {
  year_month_day cur = add_days(d, 1);
  while (!is_trading_day(market, cur)) cur = add_days(cur, 1);
  return cur;
}

std::optional<std::size_t> ffill_index(const std::vector<sys_days>& sorted_dates,
                                        sys_days target) {
  // upper_bound gives the first element > target; the ffill match is the
  // element immediately before that (exact match or nearest-prior date).
  auto it = std::upper_bound(sorted_dates.begin(), sorted_dates.end(), target);
  if (it == sorted_dates.begin()) return std::nullopt;  // target before all entries
  --it;
  return static_cast<std::size_t>(std::distance(sorted_dates.begin(), it));
}

}  // namespace bazaartalks::calendar
