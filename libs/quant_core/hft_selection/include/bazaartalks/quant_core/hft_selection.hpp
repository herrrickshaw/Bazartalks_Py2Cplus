#pragma once
// Port of hft_selection.py's pure OHLC microstructure proxies (Corwin-
// Schultz spread, Kaufman efficiency ratio, OU half-life via a 1D AR(1)
// slope, lag-1 autocorrelation) and the cross-sectional archetype-score
// composite. Report 1's own assessment: "low-moderate... all the
// microstructure-proxy math is compact numeric code, very portable --
// np.polyfit(x,y,1) is a 2-parameter least-squares fit, trivial in C++."
//
// scan_market()/pick()/main() (parquet loading, sector-basket assembly)
// are data-orchestration, not compute -- out of scope here, matching the
// same scope decision made for meta_screen.py/unlisted_valuation.py.

#include <cstddef>
#include <optional>
#include <vector>

namespace bazaartalks::quant_core {

// (High-Low)/Close per bar, NaN where Close <= 0.
std::vector<double> daily_range_pct(const std::vector<double>& high,
                                     const std::vector<double>& low,
                                     const std::vector<double>& close);

double avg_range(const std::vector<double>& high, const std::vector<double>& low,
                  const std::vector<double>& close);
double range_stability(const std::vector<double>& high, const std::vector<double>& low,
                        const std::vector<double>& close);

// Corwin & Schultz (2012) high-low spread estimator, averaged over
// consecutive 2-day pairs. NaN if fewer than 2 bars or every pair is
// unusable (non-positive low); negative per-pair estimates are floored
// to 0, matching the paper's own convention (not "fixed"/re-derived).
double corwin_schultz_spread(const std::vector<double>& high, const std::vector<double>& low);

// Kaufman efficiency ratio: |net move| / total travel, in [0,1]. NaN if
// total travel is 0 (a flat series).
double efficiency_ratio(const std::vector<double>& close);

// Lag-1 autocorrelation of `x` (finite entries only). NaN if fewer than
// 3 finite values or zero variance.
double lag1_autocorr(const std::vector<double>& x);

// Ornstein-Uhlenbeck mean-reversion half-life (in days) from the AR(1)
// slope of dP on P_{t-1}. +infinity when the series isn't (meaningfully)
// mean-reverting (slope >= -1e-9, or 1+slope <= 0), matching Python's
// `np.inf` sentinel exactly -- not an error, a real "no finite half-life"
// answer. NaN if fewer than 4 finite prices.
double ou_half_life(const std::vector<double>& close);

struct HftFeatures {
  double avg_range_pct;
  double range_stability;
  double eff_ratio;
  double ret_autocorr;
  double vol_autocorr;
  double half_life;  // may be +infinity, matching ou_half_life()
  std::optional<double> peer_corr;
  std::optional<double> peer_dev;
};

struct ArchetypeScores {
  double market_making;
  double stat_arb;
  double latency;
  std::optional<double> etf_arb;  // nullopt if no row in the batch has peer_corr
};

// Port of archetype_scores(): cross-sectional z-score composites over a
// batch of stocks' features (higher = better fit for that archetype).
// half_life is clipped to [0, 60] first (±infinity would otherwise
// dominate the z-score), matching Python's
// `.replace([inf,-inf], NaN).clip(0, 60)`.
std::vector<ArchetypeScores> archetype_scores(const std::vector<HftFeatures>& features);

}  // namespace bazaartalks::quant_core
