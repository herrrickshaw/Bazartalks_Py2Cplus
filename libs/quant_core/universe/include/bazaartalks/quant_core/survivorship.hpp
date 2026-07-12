#pragma once
// Port of survivorship.py -- point-in-time universe reconstruction and
// delisting-return splicing, removing survivorship bias from backtests.
// Pure date-range/set logic; Report 1's own assessment: "trivial-low...
// directly portable, including the splice-then-blank loop."

#include <date/date.h>

#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

struct MembershipRecord {
  std::string symbol;
  date::year_month_day add_date;
  std::optional<date::year_month_day> remove_date;  // nullopt = still a member
};

// Names in the universe on `asof`: add_date <= asof AND (no remove_date OR
// asof < remove_date). Returns a sorted, de-duplicated list -- includes
// names that later delisted (as long as they hadn't yet on `asof`) and
// excludes names not yet added, which is the whole anti-survivorship and
// anti-lookahead point.
std::vector<std::string> point_in_time_universe(const std::vector<MembershipRecord>& membership,
                                                  date::year_month_day asof);

struct DelistingInfo {
  date::year_month_day date;
  double final_return;
};

// A dense Date x Symbol returns panel, mirroring the pandas DataFrame
// apply_delisting_returns() operates on (dates ascending, symbols as
// column labels, NaN where a name doesn't trade).
struct ReturnsPanel {
  std::vector<date::year_month_day> dates;
  std::vector<std::string> symbols;
  std::vector<std::vector<double>> values;  // values[date_idx][symbol_idx]
};

// Splices each dead name's final delisting return in on its delisting
// date, then blanks (NaN) every later date for that name -- so the loss
// is counted once, and the name doesn't silently linger with stale
// pre-delisting values. Returns a NEW panel (does not mutate the input),
// matching `df.copy()` in the Python original.
ReturnsPanel apply_delisting_returns(
    const ReturnsPanel& returns,
    const std::vector<std::pair<std::string, DelistingInfo>>& delistings);

// Positive => survivors-only view overstated returns by this much (the
// bias removed by including dead names).
double survivorship_gap(double full_universe_mean, double survivors_only_mean);

struct Coverage {
  std::string asof;
  std::size_t in_universe;
  std::size_t ever_listed;
};

Coverage coverage(const std::vector<MembershipRecord>& membership, date::year_month_day asof);

}  // namespace bazaartalks::quant_core
