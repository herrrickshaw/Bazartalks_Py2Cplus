#pragma once
// Port of marketdata.py's cross-sectional statistics helpers (zscore,
// information_coefficient, monotonicity, trend_corr). These are shared by
// many later factor/DVM modules, so they live in their own utility library
// per the migration plan rather than being duplicated per-module.
//
// NaN-handling semantics are transcribed deliberately, not "cleaned up" —
// e.g. zscore() collapsing an ENTIRE degenerate cross-section (including
// originally-NaN entries) to 0.0 is a Python-side behavior callers may
// depend on; changing it would be a silent behavior change, not a bugfix.

#include <cstddef>
#include <vector>

namespace bazaartalks::stats {

// s.std(ddof=0) explicitly (population std, not pandas' sample-std default)
// — mirrors zscore()'s `s.std(ddof=0)` call exactly.
double population_stddev(const std::vector<double>& values);
double mean_skipnan(const std::vector<double>& values);

// Port of zscore(s): standardise to mean 0 / sd 1. +-inf are treated as NaN
// first. If the resulting population stddev is 0 or NaN (degenerate
// cross-section), the ENTIRE output vector becomes 0.0, matching Python's
// `pd.Series(0.0, index=s.index)` fallback (not NaN-preserving).
std::vector<double> zscore(const std::vector<double>& values);

// Pearson correlation over paired (x[i], y[i]); both vectors must be the
// same length. Returns NaN if fewer than 2 finite pairs or either side has
// zero variance (division by zero guard).
double pearson_corr(const std::vector<double>& x, const std::vector<double>& y);

// Port of information_coefficient(signal, fwd_ret): pairs the two series,
// drops rows where either side is non-finite, requires at least 10 complete
// pairs and nonzero variance on both sides, else NaN.
double information_coefficient(const std::vector<double>& signal,
                                const std::vector<double>& fwd_ret);

// pandas Series.rank(method="average"): average rank for ties, 1-indexed.
std::vector<double> rank_average(const std::vector<double>& values);

// Port of monotonicity(curve, col): correlation of the average-tie rank of
// `values` against the ideal ascending rank sequence 1..n. Requires at
// least 3 rows, else NaN.
double monotonicity(const std::vector<double>& values);

// Port of trend_corr(x): correlation of finite values against their
// position index (scale-free trend direction, in [-1, 1]). Requires at
// least 3 finite values and nonzero variance, else NaN.
double trend_corr(const std::vector<double>& values);

}  // namespace bazaartalks::stats
