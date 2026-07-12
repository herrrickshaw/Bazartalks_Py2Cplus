#pragma once
// Port of fx.py's pure conversion core (market_currency, convert_level,
// combine_return, load_rates' static-snapshot path). Report 1's own
// assessment: "trivial... currency-mapping dict + scalar conversion
// formulas; the static-snapshot fallback design is a nice pattern to keep
// in a C++ port for offline operation."
//
// fx.py's `refresh_rates()` (a live yfinance FX-pair fetch) stays in the
// Python sidecar per the migration plan -- this port covers only what's
// reachable offline: the static snapshot and the on-disk fx_rates.json
// cache the sidecar's refresh_rates() writes.
//
// `normalize_cross_market()` (a per-row DataFrame column multiply) isn't
// ported as its own function -- there's no DataFrame abstraction at this
// phase, and the substance of it (look up a market's currency, multiply
// by that currency's rate) is exactly market_currency()+convert_level();
// callers loop and call those two directly.

#include <optional>
#include <string>
#include <unordered_map>

namespace bazaartalks::quant_core {

// market (cleaned_long_*.parquet code) -> ISO currency. Matches
// fx.py's MARKET_CCY dict verbatim.
extern const std::unordered_map<std::string, std::string> kMarketCurrency;

// Static USD-per-1-unit-of-currency snapshot, dated per fx.py's own
// comment ("order-of-magnitude, dated 2026-07") -- update alongside the
// Python original if it's ever revised there.
extern const std::unordered_map<std::string, double> kSnapshotUsdPer;

// market_currency(): case-insensitive market-code lookup, defaulting to
// USD for anything not in kMarketCurrency (matches `.get(..., "USD")`).
std::string market_currency(const std::string& market);

double convert_level(double amount, double rate_base_per_local);

// Local return expressed in base currency, compounding the FX move in:
// (1+r_base) = (1+r_local)*(1+r_fx) - 1.
double combine_return(double r_local, double r_fx);

// Port of load_rates()'s static-snapshot path: for base=="USD", returns
// the snapshot as-is; for any other base, rebases every currency's
// USD-per-unit rate to base-per-unit (rounded to 6dp, matching Python's
// `round(v/b, 6)`). Does NOT read fx_rates.json here -- see
// load_rates_with_cache() for the cache-aware variant.
std::unordered_map<std::string, double> load_rates_snapshot(const std::string& base = "USD");

// Port of load_rates()'s full cache-then-snapshot-fallback logic: if
// `cache_json_path` exists, is valid JSON, and its "base" field matches
// `base`, returns its "rates" object; otherwise falls back to
// load_rates_snapshot(base). This is the file-based contract with the
// Python sidecar's refresh_rates(), which writes exactly this
// {"base": ..., "rates": {...}} shape.
std::unordered_map<std::string, double> load_rates_with_cache(
    const std::string& cache_json_path, const std::string& base = "USD");

}  // namespace bazaartalks::quant_core
