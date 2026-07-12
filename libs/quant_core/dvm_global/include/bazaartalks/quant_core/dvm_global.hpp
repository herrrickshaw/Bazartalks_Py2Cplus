#pragma once
// Port of dvm_global.py's `process_market()` -- a vectorised, OHLCV-only
// Trendlyne-style momentum/technical snapshot for markets that don't have
// fundamentals data (Durability/Valuation are US-only in the Python
// original; this module is Momentum-led with the technical filters as the
// screen, matching the parent docstring's "cross-market-computable
// subset").
//
// dvm_global.py defines TWO implementations of the same metrics: a
// per-ticker loop (`_tech()`) and a vectorised/columnar one
// (`process_market()`). They are NOT byte-identical -- `process_market()`
// is the one actually invoked by main()'s ProcessPoolExecutor fan-out, so
// this port replicates `process_market()`'s exact (sometimes narrower)
// behavior, not `_tech()`'s or the module docstring's stated intent, in
// every place they diverge. Three such divergences are called out where
// they matter below and in the .cpp:
//   1. `golden_cross` here is a single-bar-back crossover check (today's
//      50DMA>200DMA and YESTERDAY's was not), not the docstring's/`_tech()`'s
//      "crossed in the last 5 sessions".
//   2. The `dist_52w` momentum sub-score formula
//      (`100 + ((px/hi52-1)*100)*3`) is NOT the same formula as the
//      already-ported dvm_engine.py `momentum_score()`'s 52w-high
//      sub-score (`100-(hi52/px-1)*300`) -- they are different, not
//      algebraically-equivalent expressions of the same idea. Do not
//      "consolidate" them.
//   3. The vectorised beta is an internally inconsistent normalisation
//      (population covariance / sample (ddof=1) variance) -- replicated
//      verbatim, not "fixed" to a single consistent convention.

#include <cstddef>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// One ticker's OHLCV history for a market. All tickers passed to the same
// process_market() call must share the same length and be row-aligned to
// the same trading-day sequence -- this mirrors the pandas
// `df.pivot_table(index="Date", columns="Symbol", ...)` the Python
// original builds, where a ticker not yet listed on the market's earliest
// dates gets NaN-filled cells rather than a shorter column. Represent a
// not-yet-listed ticker the same way here: leading NaN in close/high/low/
// volume, not a shorter vector.
struct TickerSeries {
  std::string ticker;
  std::vector<double> close;
  std::vector<double> high;
  std::vector<double> low;
  std::vector<double> volume;
};

// The last-row (most recent date) technical/momentum snapshot for one
// ticker, matching one row of process_market()'s output DataFrame.
struct DvmGlobalMetrics {
  std::string market;
  std::string ticker;
  double M = 0.0;
  // rsi/mfi/adx/dist_52w/beta can be NaN -- callers must check std::isnan
  // before applying a numeric threshold, exactly like the Python original's
  // `(r["field"] or 0)` screen guards (see screen_* below for why a literal
  // 0-substitution isn't actually what Python's `or` does here, and why it
  // doesn't matter for these particular thresholds).
  double rsi = 0.0;
  double mfi = 0.0;
  double adx = 0.0;
  double dist_52w = 0.0;
  double vol_ratio = 0.0;
  double beta = 0.0;
  bool above_200dma = false;
  bool golden_cross = false;
  bool sma50_above_200 = false;
  bool macd_bull = false;
};

// Port of process_market(): computes the last-row snapshot for every
// ticker in `tickers`. Tickers with fewer than `min_bars` non-NaN Close
// values are dropped from the result (`c.count() >= MIN_BARS`, MIN_BARS=200
// in the Python original, exposed here as a parameter rather than
// hardcoded). A ticker whose M sub-score ends up NaN (every one of the 6
// sub-scores NaN simultaneously -- practically never, since 2 of the 6 are
// NaN-proof by construction, see the .cpp) is also dropped, matching
// `res.dropna(subset=["M"])`.
std::vector<DvmGlobalMetrics> process_market(const std::string& market,
                                              const std::vector<TickerSeries>& tickers,
                                              std::size_t min_bars = 200);

// The 6 named SCREENS predicates from dvm_global.py's SCREENS dict.
//
// Python writes several of these as `(r["field"] or 0) >= threshold`. This
// is NOT a NaN-to-zero substitution: Python's `or` only falls through to
// the right operand when the left is falsy, and `bool(float('nan'))` is
// True (NaN != 0) -- so `nan or 0` evaluates to `nan`, unchanged, and the
// ensuing comparison against a positive threshold is False purely because
// NaN comparisons are always False. The net behavior -- NaN fails these
// specific positive-threshold checks -- is exactly what a plain
// `std::isfinite(x) && x >= threshold` gives in C++, so that is what these
// implement. This equivalence is threshold-specific: it would NOT hold for
// a threshold <= 0, so don't reuse this pattern blindly if a screen with a
// non-positive bound is ever added.
bool screen_momentum_breakout(const DvmGlobalMetrics& r);
bool screen_high_momentum(const DvmGlobalMetrics& r);
bool screen_golden_crossover(const DvmGlobalMetrics& r);
bool screen_uptrend_quality(const DvmGlobalMetrics& r);
bool screen_trendlyne_technical(const DvmGlobalMetrics& r);
bool screen_sma_golden(const DvmGlobalMetrics& r);

}  // namespace bazaartalks::quant_core
