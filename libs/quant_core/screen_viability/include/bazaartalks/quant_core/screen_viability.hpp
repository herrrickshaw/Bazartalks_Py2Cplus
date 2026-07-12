#pragma once
// Port of screen_viability.py -- full-universe, forward-return viability
// backtest of 5 OHLC-computable technical screens (the `ml_bullish` 6th
// screen is NOT ported: it's optional (`--include-ml`, off by default),
// wrapped in its own try/except in the Python original, and does a
// bespoke walk-forward Ridge retrain every 10 bars -- a materially
// different, much heavier computation than ml_signal_engine.py's already-
// ported predict() path. Deferred, not silently dropped.)
//
// Scope boundary: `load_universe()` (Excel-file discovery), the SQLite
// `ticker_screen`/`done` tables and resumability, and `main()`'s
// yfinance/Cassandra `cached_download()` batching are data-plumbing, out
// of scope like every other phase's `main()`. `screens_for()` and
// `eval_ticker()` port the actual signal/backtest math; `write_summary()`'s
// SQL aggregation ports as `aggregate_market_screen_summary()`, since it's
// a pure, deterministic GROUP BY with no I/O dependency of its own.

#include <cstddef>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// One boolean signal per bar for each of the 5 named screens, all
// same-length as the input series (no NaN -- see .cpp for how each
// screen's underlying NaN inputs, e.g. RSI/DMA before their window fills,
// resolve to `false`, matching pandas' `mask.reindex(...).fillna(False)`
// in eval_ticker()).
struct ScreenSignals {
  std::vector<bool> rsi_oversold;
  std::vector<bool> near_52w_high;
  std::vector<bool> price_vol_breakout;
  std::vector<bool> darvas_proximity;
  std::vector<bool> golden_crossover;
};

// Port of screens_for(). No MIN_BARS guard here (that's eval_ticker()'s
// job) -- a series shorter than the various rolling windows just gets
// `false` for those windows' bars, matching pandas' NaN-comparison-is-
// False propagation through screens_for()'s boolean expressions.
ScreenSignals screens_for(const std::vector<double>& close, const std::vector<double>& high,
                          const std::vector<double>& low, const std::vector<double>& volume);

// One screen's summary row for one ticker. `n_signals == 0` mirrors the
// Python original's None fields (avg_fwd/hit_pct/edge) with NaN --
// `baseline` is always populated (it's the whole-series mean, independent
// of any screen).
struct ScreenEvalRow {
  std::string screen;
  int n_signals = 0;
  double avg_fwd = 0.0;   // NaN if n_signals == 0
  double hit_pct = 0.0;   // NaN if n_signals == 0
  double baseline = 0.0;
  double edge = 0.0;      // NaN if n_signals == 0
};

// Port of eval_ticker(): realised forward return over `horizon` trading
// days (`pct_change(horizon).shift(-horizon)`, clipped to +/-`clip`%),
// evaluated against each of the 5 screens' signal days. Returns an empty
// vector if close.size() < min_bars + horizon (MIN_BARS=300, FWD=5 in the
// Python original -- exposed as parameters here rather than hardcoded).
std::vector<ScreenEvalRow> eval_ticker(const std::vector<double>& close,
                                        const std::vector<double>& high,
                                        const std::vector<double>& low,
                                        const std::vector<double>& volume, int horizon = 5,
                                        double clip = 30.0, std::size_t min_bars = 300);

// One per-ticker row as stored in the Python original's `ticker_screen`
// table -- the input to the aggregation below.
struct TickerScreenRow {
  std::string market;
  std::string ticker;
  std::string screen;
  int n_signals = 0;
  double avg_fwd = 0.0;
  double hit_pct = 0.0;
  double edge = 0.0;
};

struct MarketScreenSummary {
  std::string market;
  std::string screen;
  int n_tickers = 0;
  long long total_signals = 0;
  double avg_fwd5d = 0.0;
  double avg_hit_pct = 0.0;
  double avg_edge = 0.0;
  double pct_tickers_pos_edge = 0.0;
  std::string viable;  // "YES" or "no"
};

// Port of write_summary()'s SQL: `GROUP BY market, screen` over rows with
// n_signals > 0 (rows with n_signals == 0 are excluded entirely, matching
// `WHERE n_signals > 0` -- they never reach the AVG()/COUNT() aggregates),
// ordered by market ascending then avg_edge descending, matching
// `ORDER BY market, avg_edge DESC`.
std::vector<MarketScreenSummary> aggregate_market_screen_summary(
    const std::vector<TickerScreenRow>& rows);

}  // namespace bazaartalks::quant_core
