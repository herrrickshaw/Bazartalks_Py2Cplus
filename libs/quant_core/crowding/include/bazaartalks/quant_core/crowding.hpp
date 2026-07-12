#pragma once
// Port of crowding.py's pure co-movement-crowding core: correlation to a
// market proxy plus recent relative strength, fused into a 0-100
// percentile-rank composite. Report 1's own assessment: "low -- simple
// correlation + percentile-rank computation, easily portable."
//
// Honest limit preserved from the Python original: this is a PRICE-BASED
// co-movement proxy, NOT short-interest crowding (which needs an
// exchange/vendor feed the public pipeline doesn't have).

#include <cstddef>
#include <vector>

namespace bazaartalks::quant_core {

// Correlation of a stock's trailing returns with the market proxy's.
// Requires at least 20 paired, finite observations and nonzero variance
// on both sides, else NaN -- matches corr_to_market()'s guard exactly.
double corr_to_market(const std::vector<double>& stock_ret, const std::vector<double>& market_ret);

// Trailing return over `lookback` bars: prices.back()/prices[size-lookback]
// - 1. Requires at least lookback+1 prices and a positive starting price,
// else NaN, matching rel_strength()'s guard.
double rel_strength(const std::vector<double>& prices, std::size_t lookback = 63);

struct CrowdingFeatures {
  double corr_mkt;
  double rel_strength;
};

// Cross-sectional 0-100 composite from percentile ranks of corr_mkt (65%
// weight) and rel_strength (35% weight) -- weighting co-movement over
// the run-up so "crowded" means moving with the herd, not a low-
// correlation single-name moonshot. One score per input row, in the same
// order as `features`.
//
// Assumes every row is already finite (matches scan_market()'s own
// pipeline, which only adds a row after checking `np.isfinite(cm) and
// np.isfinite(rs)`) -- pct-rank denominators here are plain vector size,
// not a NaN-excluding count, since this function is never called with
// NaN rows in the Python original either.
std::vector<double> crowding_score(const std::vector<CrowdingFeatures>& features);

}  // namespace bazaartalks::quant_core
