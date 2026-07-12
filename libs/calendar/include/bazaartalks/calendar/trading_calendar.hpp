#pragma once
// Port of market_holidays.py — a lightweight, no-external-deps multi-market
// trading calendar. This is a speed filter, not a settlement-grade calendar
// (see the Python original's own docstring); ported 1:1 so behavior matches.

#include <date/date.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bazaartalks::calendar {

enum class Market { US, India, Japan, Korea, Europe };

// Resolves market aliases (NSE/BSE -> India, NYSE/NASDAQ -> US, TSE/JPX ->
// Japan, KRX/KOSPI -> Korea, EU/XETRA -> Europe) the same way Python's
// ALIASES dict + _canon() do. Throws std::invalid_argument on an unknown key.
Market market_from_alias(std::string_view market_or_alias);

// Gregorian Easter (Anonymous/Meeus algorithm) — same arithmetic as
// market_holidays.py's _easter(), used to derive Good Friday/Easter Monday
// for Europe.
date::year_month_day easter(int year);

bool is_trading_day(Market market, date::year_month_day d);
inline bool is_trading_day(std::string_view market, date::year_month_day d) {
  return is_trading_day(market_from_alias(market), d);
}

// All trading sessions for a market in [start, end], inclusive, ascending.
std::vector<date::year_month_day> trading_days(Market market,
                                                 date::year_month_day start,
                                                 date::year_month_day end);

date::year_month_day next_trading_day(Market market, date::year_month_day d);

// --- Point-in-time date-snapping utility ---------------------------------
//
// Replicates pandas' `idx.get_indexer([target], method="ffill")` semantics,
// used throughout pit_backtest.py/pit_global.py to snap a target date to
// the most recent known index date at or before it. This single utility is
// reused by every PIT/backtest module — the tie-breaking rule below is
// load-bearing for correctness there:
//
//   - if `target` is present in `sorted_dates`, return its index.
//   - else return the index of the largest date strictly less than target.
//   - if no such date exists (target is before every entry), return
//     std::nullopt (mirroring pandas returning -1 for an unmatched ffill).
//
// `sorted_dates` MUST be sorted ascending with no duplicates (as
// pandas.DatetimeIndex is in the modules that use this pattern).
std::optional<std::size_t> ffill_index(
    const std::vector<date::sys_days>& sorted_dates, date::sys_days target);

}  // namespace bazaartalks::calendar
