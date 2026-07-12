#pragma once
// Port of portfolio.py's pure optimiser core (and the same pinv-based
// min-variance/max-Sharpe formulas factor_research.py's markowitz() test
// uses) -- covariance-based portfolio weights, long-only clipping,
// per-position/per-sector caps (iterative constraint satisfaction), and
// turnover-budget blending vs a previous book.

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace bazaartalks::quant_core {

using Vector = Eigen::VectorXd;
using Matrix = Eigen::MatrixXd;

// w = Sigma^-1 * 1 / (1' * Sigma^-1 * 1), via bt_linalg's pinv (matching
// numpy.linalg.pinv's rcond convention, not a naive inverse).
Vector min_variance_weights(const Matrix& cov);

// Tangency portfolio (no risk-free): w ∝ Sigma^-1 * mu. Falls back to
// equal-weight if the raw weights sum to exactly 0 (a degenerate case
// Python guards against explicitly, not just "let it divide by zero").
Vector max_sharpe_weights(const Vector& mu, const Matrix& cov);

// Clips shorts to zero and renormalises; falls back to equal-weight if
// every weight is <= 0 (matches Python's `if s > 0 else ones/n`).
Vector long_only(const Vector& w);

// Enforces a per-position cap, iteratively pushing spillover onto
// uncapped names until every weight <= cap. Returns equal-weight
// immediately if `cap * n < 1` (infeasible -- the closest feasible
// portfolio Python falls back to, not an error).
Vector cap_weights(const Vector& w, double cap, int iters = 100);

// Caps the summed weight of any one sector at `cap`, redistributing the
// excess proportionally to names in under-cap sectors. `sectors[i]` is
// the sector label for `w[i]`.
Vector apply_sector_cap(const Vector& w, const std::vector<std::string>& sectors, double cap,
                        int iters = 100);

// One-way turnover = 0.5 * sum(|w_new - w_old|) (0 = identical book, 1 =
// full replacement).
double turnover(const Vector& w_new, const Vector& w_old);

// Moves from w_old toward w_new only as far as the turnover budget
// allows: w = w_old + lambda*(w_new - w_old), lambda chosen so realised
// turnover <= max_turnover. Returns w_new unchanged if already within
// budget (or if turnover is exactly 0, avoiding a 0/0 lambda).
Vector blend_to_turnover(const Vector& w_new, const Vector& w_old, double max_turnover);

}  // namespace bazaartalks::quant_core
