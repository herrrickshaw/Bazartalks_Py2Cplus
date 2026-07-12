#include "bazaartalks/quant_core/survivorship.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
}

std::vector<std::string> point_in_time_universe(const std::vector<MembershipRecord>& membership,
                                                  date::year_month_day asof) {
  date::sys_days t = date::sys_days{asof};
  std::set<std::string> out;
  for (const auto& m : membership) {
    date::sys_days add = date::sys_days{m.add_date};
    bool in_range = add <= t;
    if (in_range && m.remove_date) {
      in_range = t < date::sys_days{*m.remove_date};
    }
    if (in_range) out.insert(m.symbol);
  }
  return std::vector<std::string>(out.begin(), out.end());  // std::set already sorted
}

ReturnsPanel apply_delisting_returns(
    const ReturnsPanel& returns,
    const std::vector<std::pair<std::string, DelistingInfo>>& delistings) {
  ReturnsPanel out = returns;  // `df.copy()`

  std::unordered_map<std::string, std::size_t> symbol_col;
  for (std::size_t j = 0; j < out.symbols.size(); ++j) symbol_col[out.symbols[j]] = j;

  for (const auto& [sym, info] : delistings) {
    auto col_it = symbol_col.find(sym);
    if (col_it == symbol_col.end()) continue;  // `if sym not in df.columns: continue`
    std::size_t col = col_it->second;
    date::sys_days ddate = date::sys_days{info.date};

    for (std::size_t i = 0; i < out.dates.size(); ++i) {
      date::sys_days dt = date::sys_days{out.dates[i]};
      if (dt == ddate) {
        out.values[i][col] = info.final_return;
      } else if (dt > ddate) {
        out.values[i][col] = kNaN;
      }
    }
  }
  return out;
}

double survivorship_gap(double full_universe_mean, double survivors_only_mean) {
  return survivors_only_mean - full_universe_mean;
}

Coverage coverage(const std::vector<MembershipRecord>& membership, date::year_month_day asof) {
  auto live = point_in_time_universe(membership, asof);
  std::set<std::string> ever;
  for (const auto& m : membership) ever.insert(m.symbol);
  return Coverage{date::format("%F", asof), live.size(), ever.size()};
}

}  // namespace bazaartalks::quant_core
