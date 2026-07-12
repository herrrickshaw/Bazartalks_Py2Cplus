#pragma once
// Port of quality_factor.py's AFP/QMJ (Asness-Frazzini-Pedersen) quality
// score: rank -> z-score -> dimension-average -> re-standardize pipeline,
// decile assignment, value-weighting, and the price-premium regression.
//
// Named trap (per the migration plan, called out explicitly rather than
// discovered mid-port): price_premium()'s market fixed effects use
// `pd.get_dummies(market, drop_first=True)`, which (a) sorts categories
// ALPHABETICALLY, not by first appearance, and (b) drops the
// alphabetically-FIRST category as the reference level. Getting either
// wrong silently shifts every fixed-effect coefficient's baseline without
// erroring -- replicated exactly in build_market_dummies() below.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// AFP "standardised rank": cross-sectional rank (average-tie method),
// then z-scored with POPULATION std (ddof=0). Unlike
// bazaartalks::stats::zscore, a degenerate (zero-variance) cross-section
// returns all-NaN here, NOT all-zero -- this is z_rank()'s own semantic,
// not a bug to reconcile with the shared zscore() helper.
std::vector<double> z_rank(const std::vector<double>& values);

// One row's raw sub-metric inputs. NaN represents a missing/absent
// column (all of z_rank's sub-metrics are optional per-stock; dimension
// scoring skips whichever aren't present, matching Python's
// `if col in df.columns` gate applied at the DataFrame-column level,
// generalized here to a per-value NaN check since every column always
// "exists" in a fixed C++ struct).
struct QualityInputs {
  double roe, roa, op_margin;           // profitability
  double rev_growth, earn_growth;       // growth
  double de, beta, vol;                 // safety (de/beta/vol all lower-is-better)
  double div_yield;                     // payout
  double mktcap;
  double pb;
};

struct QualityScore {
  double profitability;
  double growth;
  double safety;
  double payout;
  double quality_raw;
  double quality;
  double quality_score;  // 0-100, rounded to 1dp
};

// Full quality_score() pipeline over a cross-section (one market's worth
// of stocks, or a pooled cross-market batch -- the caller decides scope,
// matching Python's `score_universe(by_market=...)` toggle).
std::vector<QualityScore> quality_score_batch(const std::vector<QualityInputs>& rows);

// Labels each stock quality/junk/mid by percentile rank of its quality
// score within the batch (top >= 0.90 -> "quality", bottom <= 0.10 ->
// "junk", else "mid").
std::vector<std::string> assign_deciles(const std::vector<double>& quality_scores,
                                        double top = 0.90, double bot = 0.10);

// Market-cap value-weighting within a leg: w = clip(mktcap,0)/sum, or
// equal-weight if the clipped sum is 0 (matches Python's fallback).
std::vector<double> value_weight(const std::vector<double>& mktcap);

struct PricePremiumResult {
  std::size_t n;
  std::optional<double> quality_coef;
  std::optional<double> quality_t;
  std::optional<double> mb_premium_per_sd_pct;
  std::optional<double> r_squared;
};

// Cross-sectional regression log(pb) ~ quality + log(mktcap) + market
// fixed effects, reproducing the paper's Table 8 test. Filters to
// pb>0 & mktcap>0 & all three non-NaN internally (matching Python's
// `dropna(subset=[...])` + positivity filter), then takes logs.
// `quality_coef` is per +1 SD of the (already-standardised) quality
// score; `mb_premium_per_sd_pct` = (exp(coef)-1)*100. Returns
// {n, nullopt...} if fewer than 30 rows survive the filter, matching
// Python's early return.
PricePremiumResult price_premium(const std::vector<double>& pb, const std::vector<double>& quality,
                                  const std::vector<double>& mktcap,
                                  const std::vector<std::string>& market);

struct DriverCorrelation {
  std::string dimension;
  double corr_with_log_mb;
};

// Correlation of each quality dimension with log(pb), sorted descending
// -- which dimensions the market actually pays up for (paper finds
// profitability & payout dominate). Rows with pb<=0 or NaN pb are
// dropped, matching Python's `dropna(subset=["pb"])` + `pb>0` filter.
std::vector<DriverCorrelation> driver_breakdown(const std::vector<double>& profitability,
                                                 const std::vector<double>& growth,
                                                 const std::vector<double>& safety,
                                                 const std::vector<double>& payout,
                                                 const std::vector<double>& pb);

}  // namespace bazaartalks::quant_core
