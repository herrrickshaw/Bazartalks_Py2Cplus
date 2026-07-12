#pragma once
// Port of pit_global.py -- extends pit_backtest.py's genuinely lookahead-
// free (prices-only) Darvas-breakout backtest to every market, since only
// the US has the SEC-EDGAR filed-date history needed for the Piotroski/
// Coffee-Can fundamental gates. reuses libs/quant_core/pit_backtest's
// monthly_rebalance_dates()/period_returns()/compute_metrics() directly --
// pit_global.py's own date-snap idiom (`idx[idx <= t0][-1]`) is the same
// ffill operation as pit_backtest.py's `get_indexer(method="ffill")`
// expressed differently, not a behavioral difference, so there is no
// separate engine to re-derive here.
//
// Honesty preserved from the Python original: the `overlay` arm
// (D_darvas_durable) uses CURRENT fundamentals as a STATIC durability
// gate -- it is look-ahead, not point-in-time, and callers must present
// it as such (matching main()'s own printed warning). Only A_darvas and
// BENCH_all are clean PIT arms.
//
// Scope boundary: `load_closes()`'s parquet I/O and `durable_names()`'s
// fundamentals_cache.db read are data-plumbing, out of scope like every
// other phase's I/O. Only the pure predicate/lookup/aggregation logic
// ports; callers supply the durable-ticker set and per-ticker OHLC
// history directly.

#include "bazaartalks/quant_core/pit_backtest.hpp"

#include <set>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// Port of market_cost(): apply_costs.py's COSTS_PCT dict (percentage-point
// round-trip cost), reached via pit_global.py's own COST_BUCKET
// indirection (cleaned_long market code -> apply_costs cost-bucket name).
// Falls back to 0.30 for any market not in COST_BUCKET, matching
// `COSTS_PCT.get(COST_BUCKET.get(market, ""), 0.30)`. Distinct from
// net_of_cost.py's ROUND_TRIP dict (see cost_models/net_of_cost.hpp) --
// same real-world costs, different module/units, not consolidated.
double market_cost(const std::string& market);

// Port of durable_names()'s STATIC (NOT point-in-time) durability
// predicate: roe > 12 and de < 1.5 using CURRENT fundamentals. NaN
// inputs (a coerced-to-NaN fundamentals field in the Python original)
// naturally fail both comparisons, same as pandas.
bool is_durable(double roe, double de);

// `tkr.split(".")[0].upper()` -- strips a yfinance-style exchange suffix
// (e.g. "7203.T" -> "7203") to match the durable-names set's own keying.
std::string base_symbol_upper(const std::string& ticker);

struct MarketArmResult {
  std::string market;
  std::string arm;
  BacktestMetrics metrics;
};

// Port of run_market(): returns {} if tickers.size() < 20, matching the
// Python original's `if len(closes) < 20: return pd.DataFrame()` data-
// quality guard. Output order matches Python's dict-insertion order:
// A_darvas, BENCH_all, then D_darvas_durable (only present if overlay).
// `durable` must be keyed by base_symbol_upper() (the caller's job, same
// as the real durable_names()'s own output format) and is ignored unless
// overlay is true.
std::vector<MarketArmResult> run_market(const std::string& market,
                                        const std::vector<TickerHistory>& tickers, bool overlay,
                                        const std::set<std::string>& durable = {},
                                        std::size_t darvas_lookback = 60,
                                        std::size_t min_lookback = 60);

}  // namespace bazaartalks::quant_core
