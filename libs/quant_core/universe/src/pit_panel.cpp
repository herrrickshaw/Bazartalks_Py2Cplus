#include "bazaartalks/quant_core/pit_panel.hpp"

#include <algorithm>

namespace bazaartalks::quant_core {

std::unordered_map<std::string, std::size_t> as_of(const std::vector<FiledRecord>& panel,
                                                    date::year_month_day asof) {
  date::sys_days t = date::sys_days{asof};
  std::unordered_map<std::string, std::size_t> best;
  for (std::size_t i = 0; i < panel.size(); ++i) {
    const auto& r = panel[i];
    date::sys_days fd = date::sys_days{r.filed_date};
    if (fd > t) continue;
    auto it = best.find(r.symbol);
    if (it == best.end() || fd > date::sys_days{panel[it->second].filed_date}) {
      best[r.symbol] = i;
    }
  }
  return best;
}

bool leaks_lookahead(const std::vector<FiledRecord>& panel, date::year_month_day asof) {
  date::sys_days t = date::sys_days{asof};
  for (const auto& r : panel) {
    if (date::sys_days{r.filed_date} > t) return true;
  }
  return false;
}

bool min_history_ok(int n_days, double min_years, int trading_days_year) {
  return static_cast<double>(n_days) >= min_years * static_cast<double>(trading_days_year);
}

MarketGate gate_markets(const std::unordered_map<std::string, int>& history_days,
                        double min_years) {
  std::vector<std::string> markets;
  markets.reserve(history_days.size());
  for (const auto& [mkt, _] : history_days) markets.push_back(mkt);
  std::sort(markets.begin(), markets.end());

  MarketGate gate;
  gate.min_years = min_years;
  gate.min_days = static_cast<int>(min_years * kTradingDaysPerYear);
  for (const auto& mkt : markets) {
    if (min_history_ok(history_days.at(mkt), min_years)) {
      gate.report.push_back(mkt);
    } else {
      gate.withhold.push_back(mkt);
    }
  }
  return gate;
}

}  // namespace bazaartalks::quant_core
