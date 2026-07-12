#pragma once
// Port of pit_backtest.py -- the platform's genuinely lookahead-free
// backtest engine. This IS the correctness-critical Phase 6 gate: a
// subtle off-by-one here (an entry/exit date snapped to the wrong side of
// a rebalance boundary, or a breakout check that peeks at a future bar)
// produces a backtest that still runs and looks plausible while being
// silently wrong.
//
// Scope boundary: `piotroski_asof()`/`coffeecan_asof()`/`as_of()` (arms B
// and C's Piotroski-F/Coffee-Can gates) live in pit_fundamentals.py,
// which needs live/cached SEC EDGAR filed-date data -- out of scope here,
// same as every other phase's external-data boundary. What ports is the
// PIT-sensitive ENGINE itself: monthly rebalance-date generation, the
// per-ticker ffill date-snap (each ticker snapped against its OWN listing
// history, not a shared index -- a ticker missing data at either end of a
// period is simply absent from that period, never backfilled or
// forward-filled from unrelated dates), the forward-return computation,
// and the Darvas-breakout signal. Arms B/C are the CALLER's job: filter
// period_returns()'s output by ticker using an externally-supplied,
// already-as-of-t0 gate (e.g. bazaartalks::quant_core::as_of() in
// libs/quant_core/universe/pit_panel.hpp, which is itself already
// golden-tested for the same anti-lookahead guarantee this module needs).
//
// `load_scan_universe()`'s Excel-file discovery and `main()`'s
// `cached_download()`/SQLite-writing orchestration are data-plumbing, out
// of scope like every other phase's `main()`.

#include <date/date.h>

#include <cstddef>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// Port of darvas_breakout(): true iff close[loc] is a new high vs the
// prior `lookback` closes (a Darvas-box breakout proxy). `loc` is the
// caller-resolved integer position of the entry bar within `close`
// (equivalent to Python's `close.index.get_loc(t)`); false if
// loc < lookback (not enough prior history to compare against), matching
// the Python guard `if loc < BREAKOUT_LB: return False`.
bool darvas_breakout(const std::vector<double>& close, std::size_t loc,
                     std::size_t lookback = 60);

struct BacktestMetrics {
  int n_months = 0;
  double avg_month_pct = 0.0;
  double ann_return_pct = 0.0;
  double hit_rate_pct = 0.0;
  double sharpe = 0.0;
  double max_dd_pct = 0.0;
};

// Port of metrics(): n_months == 0 (all other fields left at their
// default) iff monthly_returns_pct is empty, matching Python's bare
// `{"n_months": 0}` dict for that case.
BacktestMetrics compute_metrics(const std::vector<double>& monthly_returns_pct);

// One ticker's full OHLC(Close-only) history. `dates` must be ascending
// with no duplicates, matching a pandas Series' DatetimeIndex -- and may
// start later (or end earlier) than other tickers passed to the same
// call, exactly like the Python original's per-ticker `pd.Series`, each
// with its own independent DatetimeIndex.
struct TickerHistory {
  std::string ticker;
  std::vector<date::sys_days> dates;
  std::vector<double> close;
};

// One ticker's result for a single (t0, t1) rebalance period.
struct PeriodReturn {
  std::string ticker;
  double forward_return_pct = 0.0;
  bool is_darvas_breakout = false;
};

// The per-period inner loop: for every ticker, ffill-snap t0 and t1
// against THAT TICKER's OWN date index (never a shared/global one), skip
// the ticker entirely if either snap fails (no date <= t0, or no date <=
// t1) or if the snapped entry date is not strictly before the snapped
// exit date, compute the forward return between the two snapped closes,
// and skip a non-finite result (division by a zero/degenerate price).
// Matches pit_backtest.py's per-ticker loop body inside main() exactly,
// including its `if not np.isfinite(fwd): continue` guard against
// gaps/split glitches poisoning the mean.
std::vector<PeriodReturn> period_returns(const std::vector<TickerHistory>& tickers,
                                          date::sys_days t0, date::sys_days t1,
                                          std::size_t darvas_lookback = 60);

// Generates monthly rebalance dates: the first trading day of each
// calendar month present in the union of every ticker's dates, restricted
// to the (min_lookback+1)-th date onward in that union (0-indexed) --
// matching `cal.resample("MS").first().index` filtered by
// `d >= all_idx[BREAKOUT_LB + 1]` (pit_backtest.py) /
// `d >= all_idx[61]` (pit_global.py, same cutoff for the same default
// lookback of 60). Returns dates in ascending order with no duplicates.
std::vector<date::sys_days> monthly_rebalance_dates(const std::vector<TickerHistory>& tickers,
                                                     std::size_t min_lookback = 60);

}  // namespace bazaartalks::quant_core
