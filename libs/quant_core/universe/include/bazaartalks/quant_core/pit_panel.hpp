#pragma once
// Port of pit_panel.py -- market-agnostic point-in-time filed-date logic
// (L1: as_of filtering) plus a minimum-history reporting gate (L5). Pure
// dict/list logic with no numpy/pandas in the Python original.
//
// Design note: Python's `as_of(panel, T)` returns {symbol: row} where
// `row` carries arbitrary caller-defined metric columns beyond
// symbol/filed_date. C++ has no equivalent to a dynamically-shaped dict
// row, so this returns the WINNING ROW INDEX per symbol instead -- callers
// keep their own metric data in an index-correlated vector/struct and look
// up the winning index here. This is exactly the same scope decision the
// migration plan makes for pit_fundamentals.py: "port only the as-of
// filtering logic... callers supply/consume the metric payload."

#include <date/date.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace bazaartalks::quant_core {

struct FiledRecord {
  std::string symbol;
  date::year_month_day filed_date;
};

// L1: latest row filed on/before `asof`, per symbol. Rows filed AFTER
// asof are excluded (the point-in-time guarantee); among rows filed
// on/before asof, the one with the greatest filed_date wins (a later
// restatement of the same fact, still known by `asof`). Returns
// {symbol -> index into `panel`}.
std::unordered_map<std::string, std::size_t> as_of(const std::vector<FiledRecord>& panel,
                                                    date::year_month_day asof);

// True iff the panel contains any figure that would NOT have been known
// at `asof` -- i.e. any row with filed_date > asof.
bool leaks_lookahead(const std::vector<FiledRecord>& panel, date::year_month_day asof);

constexpr int kTradingDaysPerYear = 252;

// L5: true iff `n_days` of history meets the minimum-years bar.
bool min_history_ok(int n_days, double min_years = 3.0,
                     int trading_days_year = kTradingDaysPerYear);

struct MarketGate {
  std::vector<std::string> report;
  std::vector<std::string> withhold;
  double min_years;
  int min_days;
};

// Splits markets into report/withhold by history length -- a blank beats
// a bad guess. `history_days` need not be pre-sorted; markets are
// processed in ascending name order, matching Python's `sorted(...)`.
MarketGate gate_markets(const std::unordered_map<std::string, int>& history_days,
                        double min_years = 3.0);

}  // namespace bazaartalks::quant_core
