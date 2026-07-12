#pragma once
// Port of factor_research.py's markowitz() (P1: does diversification beat
// a concentrated pick / equal-weight on realised Sharpe?). Reuses
// portfolio.py's exact min-variance/max-Sharpe formulas (libs/quant_core/
// portfolio) since both modules build the identical pinv(Sigma)-based
// portfolios -- this is the composition/backtest-scoring layer on top.
//
// `ols()` itself is NOT re-ported here -- it's already Phase 1's
// `bazaartalks::linalg::ols_with_stats`, byte-for-byte the same normal-
// equation formula factor_research.py's ols() uses.
//
// `quintile_table()` (pd.qcut-based bucketing) and `cross_section()`
// (OHLC/PIT-fundamentals panel assembly) are NOT ported: quintile_table
// is a print-only reporting aid consumed by nothing else in the pipeline
// (replicating pandas' internal qcut bin-edge/epsilon adjustments exactly
// would be real effort for a function with no downstream consumer), and
// cross_section is data-orchestration belonging to the wider pipeline
// (Phase 6+), not compute core.

#include "bazaartalks/quant_core/portfolio.hpp"

namespace bazaartalks::quant_core {

struct PortfolioStats {
  double fwd_ret_pct;
  double ex_ante_vol_pct;
  double ret_over_vol;  // 0 if ex_ante_vol_pct is 0, matching Python's `if vol else 0`
};

struct MarkowitzResult {
  PortfolioStats concentrated;
  PortfolioStats equal_weight;
  PortfolioStats min_variance;
  PortfolioStats max_sharpe;
};

// `mu`/`cov` should already be annualised (Python's own convention:
// `R.mean().values * 252`, `R.cov().values * 252`). `fwd_ret_pct` is each
// asset's REALISED forward return in percent (already known at scoring
// time -- this is a point-in-time backtest comparison, not a live
// decision), in the same order as mu/cov's rows/columns.
MarkowitzResult markowitz_portfolios(const Vector& mu, const Matrix& cov,
                                      const Vector& fwd_ret_pct);

}  // namespace bazaartalks::quant_core
