#pragma once
// Port of dvm_composite.py -- the capstone that fuses Momentum (technical,
// from local OHLC) with Durability + Valuation (fundamental) into one
// GGG/GGB/.../BBB composite ranking across every market at once.
//
// dvm_composite.py's `momentum(c)` and `durability(r)` are NEW, DISTINCT
// formulas from both dvm_engine.py's momentum_score()/durability_score()
// and dvm_global.py's process_market() -- not aliases or refactors of
// them. Do not consolidate:
//   - `momentum(c)` takes ONLY a Close-price series (no High/Low/Volume),
//     so it has no ADX or volume-thrust sub-score -- just 4 subs (RSI,
//     MACD sign, DMA stack, 52w-high proximity). Its MIN_BARS-equivalent
//     guard is 200 (not dvm_engine.py's 220). Its dist_52w sub-formula is
//     `100 + (px/hi52-1)*300` (dvm_global.py's form), NOT dvm_engine.py's
//     `100-(hi52/px-1)*300`. Its 52w-high window is
//     `rolling(252, min_periods=150)` (dvm_global.py's form), not
//     dvm_engine.py's plain `rolling(252)`. Like dvm_engine.py's
//     momentum_score() (and unlike dvm_global.py's process_market()), its
//     clipping is Python's SCALAR builtin min()/max() (NaN collapses to 0
//     via first-argument-wins comparisons), not numpy's element-wise,
//     NaN-propagating np.minimum/np.maximum.
//   - `durability(r)` has its own 5 possible sub-scores (roe/de/
//     rev_growth/op_margin/earn_growth) with thresholds that don't match
//     dvm_engine.py's durability_score() (different tiers, different
//     breakpoints, no Piotroski-F term at all here). Critically, it only
//     APPENDS a sub-score for a field that's actually present (`if
//     pd.notna(field): subs.append(...)`) rather than substituting a
//     default for a missing one -- the mean is over however many subs got
//     appended (2 to 5), and if NONE are present the result is NaN. This
//     is a genuinely different missing-data policy from
//     dvm_engine.py's durability_score() (which always averages exactly
//     5 subs, substituting defaults for missing inputs).
//
// Scope boundary: `main()`'s fundamentals_cache.db/cleaned_long-parquet
// I/O is data-plumbing, out of scope (same boundary as every other
// phase). `build_dvm_composite()` below ports everything downstream of
// that I/O: the per-stock momentum/durability fusion, the drop-if-either-
// missing filter, the GLOBAL (all-markets-combined, not per-market)
// cross-sectional earnings-yield/inverse-P/B valuation rank, and the
// round-then-classify-then-composite finish -- including the specific
// ordering quirk that D/M/V are rounded to 1 decimal BEFORE the G/B
// classification and the composite average, so a value that rounds
// across the 50 boundary (e.g. 49.96 -> 50.0) changes classification
// from what the unrounded value would have given.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// Port of momentum(c): 0-100 technical bullishness from Close prices only.
// Returns NaN if close.size() < 200.
double dvm_composite_momentum(const std::vector<double>& close);

// Port of durability(r): mean of whichever of the 5 possible sub-scores
// have a present (non-nullopt) input; NaN if none are present.
double dvm_composite_durability(std::optional<double> roe, std::optional<double> de,
                                std::optional<double> rev_growth,
                                std::optional<double> op_margin,
                                std::optional<double> earn_growth);

// One candidate stock's raw inputs, before the momentum/durability fusion
// and cross-sectional valuation ranking.
struct DvmCompositeInput {
  std::string market;
  std::string ticker;
  std::string sector;
  std::vector<double> close;
  std::optional<double> roe;
  std::optional<double> de;
  std::optional<double> rev_growth;
  std::optional<double> op_margin;
  std::optional<double> earn_growth;
  std::optional<double> pe;  // earnings yield = 1/pe if pe present and > 0
  std::optional<double> pb;
};

// One row of the finished composite table (only rows with both M and D
// defined survive -- `if pd.isna(M) or pd.isna(D): continue`).
struct DvmCompositeRow {
  std::string market;
  std::string ticker;
  std::string sector;
  double M = 0.0;  // already rounded to 1 decimal, matching df["M"].round(1)
  double D = 0.0;  // already rounded to 1 decimal
  double V = 0.0;  // already rounded to 1 decimal
  double composite = 0.0;
  std::string code;
  std::string label;
};

// Port of main()'s per-stock fusion + cross-sectional valuation ranking +
// classification, EXCLUDING the fundamentals_cache.db/parquet I/O that
// supplies `inputs` in the real pipeline. The valuation rank (and its
// fillna-with-column-mean step) is computed across ALL surviving rows in
// `inputs` at once, spanning every market together -- matching main()'s
// actual behavior of ranking the combined `df` after the per-market
// momentum/durability loop, not ranking within each market separately.
std::vector<DvmCompositeRow> build_dvm_composite(const std::vector<DvmCompositeInput>& inputs);

}  // namespace bazaartalks::quant_core
