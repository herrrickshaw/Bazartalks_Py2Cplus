#pragma once
// Port of dvm_engine.py -- Trendlyne-style Durability/Valuation/Momentum
// (DVM) scoring, GGG/GGB/... composite classification, and the module's
// 4 named SCREENS.
//
// Scope boundary (matches the established Phase 3/4 pattern): only the
// pure scoring math ports here.
//   - `load_universe()` (Excel-file discovery under ~/Downloads) and
//     `main()`'s `cached_download()`/SQLite-writing orchestration are
//     data-plumbing, not compute -- out of scope, same as every other
//     phase's CLI/`main()`.
//   - `durability_score()`'s point-in-time EDGAR fetch (`pit_fundamentals.
//     as_of()`, `piotroski_asof()`) is correctness-critical PIT logic
//     reserved for Phase 6, not reimplemented here. Only the function's
//     own closed-form SCORING step (roe/de/fcf/rev-growth/Piotroski-F ->
//     one 0-100 number) ports now, taking those five inputs as plain
//     scalars/optionals instead of fetching them.
//   - Valuation (`V`, the earnings-yield cross-sectional rank in `main()`)
//     is a cross-sectional rank over the whole scored universe, not a
//     per-ticker pure function -- deferred to whichever phase ports the
//     orchestration loop that also needs the live EDGAR/market-cap feed.
//
// `momentum_score()` here is DELIBERATELY NOT reused from dvm_global.py's
// process_market() port (libs/quant_core/dvm_global) even though they
// share RSI/MACD/ADX-DMI kernels (bazaartalks::stats) -- the two Python
// modules' composite formulas genuinely differ:
//   - MIN_BARS-equivalent guard: 220 bars here vs dvm_global.py's 200.
//   - 52w-high window: plain `rolling(252).max()` here (no min_periods
//     override) vs dvm_global.py's `rolling(252, min_periods=150)`.
//   - dist_52w sub-score formula: `100-(hi52/px-1)*300` here vs
//     dvm_global.py's `100+((px/hi52-1)*100)*3` -- not algebraically
//     equivalent, see dvm_global.hpp's own note on this.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// Port of momentum_score(): 0-100 technical bullishness composite from a
// single ticker's OHLCV history. Returns NaN if close.size() < 220,
// matching `if df is None or len(df) < 220: return np.nan`.
double momentum_score(const std::vector<double>& close, const std::vector<double>& high,
                      const std::vector<double>& low, const std::vector<double>& volume);

// Port of durability_score()'s pure scoring formula (the module's lines
// computing `subs` from already-known roe/de/fcf/rev_g/piotroski-F, NOT
// the EDGAR `as_of()`/`piotroski_asof()` calls that produce those inputs
// -- see the header note above). `roe`/`de`/`fcf_positive`/`piotroski_f`
// are std::optional to mirror Python's `if roe and roe > 20`/`is not None`
// checks; `revenue_growing` is a plain bool since the Python original's
// `rev_g` is always a concrete bool (never None).
double durability_score(std::optional<double> roe, std::optional<double> de,
                        std::optional<bool> fcf_positive, bool revenue_growing,
                        std::optional<double> piotroski_f);

// Port of `def g(x): return "G" if (x is not None and x >= 50) else "B"`.
// Note this is a `has_value()` check, NOT the truthy/falsy check the
// SCREENS predicates below use for the same D/V fields -- x == 0.0 (present,
// not None) yields "B" here, whereas a SCREENS predicate's `(r["D"] or 0)`
// idiom treats a present-but-zero D the same as an absent one. Both are
// replicated exactly as their respective Python lines do; don't
// consolidate them into one "is D usable" helper.
char classification_letter(std::optional<double> x);

// LABELS dict lookup (e.g. "GGG" -> "Strong Performer"); "-" for any code
// not in the table (every G/B permutation actually IS covered, so this
// only matters for a caller-supplied code outside {G,B}^3).
std::string classification_label(const std::string& code);

// One row of dvm_engine.py's output dict, as seen by the SCREENS lambdas.
// D/V are std::optional because the Python row dict genuinely stores
// `None` for a non-US market or missing fundamentals (unlike dvm_global.py,
// where an unscoreable field stays a float NaN) -- see dvm_global.hpp's
// screen_* comment for why that distinction matters for the `or 0` idiom.
struct DvmRow {
  std::optional<double> D;
  std::optional<double> V;
  double M = 0.0;
};

// The 4 named SCREENS predicates. NOTE: `screen_momentum_breakout` here
// shares its name AND its M>=75 threshold with dvm_global.py's
// `screen_high_momentum` (not dvm_global.py's own, differently-defined,
// `screen_momentum_breakout` overload on DvmGlobalMetrics) -- the two
// Python modules independently chose the same SCREENS key name for
// different predicates; both C++ overloads are kept, deliberately not
// merged, to mirror that the two Python dicts are genuinely separate.
bool screen_high_dvm(const DvmRow& r);
bool screen_durable_momentum(const DvmRow& r);
bool screen_value_under_radar(const DvmRow& r);
bool screen_momentum_breakout(const DvmRow& r);

}  // namespace bazaartalks::quant_core
