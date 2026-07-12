#pragma once
// NEW RISK OVERLAY, NOT A PORT: Mahalanobis-distance-based market
// turbulence index (Kritzman & Li 2010), used by the ensemble deep-RL
// trading paper reviewed alongside arXiv:2412.14361v1 as a crash-
// detection trigger to halt new positions. The turbulence index itself
// is pure linear algebra -- no RL/neural-net training involved, unlike
// the rest of that paper's methodology, which is why this piece (and
// only this piece) was extracted as buildable: it pairs naturally with
// this repo's existing risk.hpp regime_flag() as an additional,
// cross-sectional (multi-asset) regime signal, versus regime_flag()'s
// single-series vol-ratio/drawdown check.
//
// VALIDATION NOTE: as with momentum_breakout.hpp, this is new C++-native
// code with no existing Python original -- tests cross-check against an
// independent numpy implementation of the same formula, not "the actual
// Python output."

#include <Eigen/Dense>

#include <vector>

namespace bazaartalks::quant_core {

// turbulence_t = (y_t - mu)' * Sigma^-1 * (y_t - mu)
// where y_t is the current period's return vector across N assets, mu is
// the historical mean return vector, and Sigma is the historical
// covariance matrix. Uses bazaartalks::linalg::pinv (not a naive
// inverse) for the same reason portfolio.py's optimizers do -- Sigma can
// be near-singular with correlated/redundant assets.
double turbulence_index(const Eigen::VectorXd& returns_t, const Eigen::VectorXd& historical_mean,
                        const Eigen::MatrixXd& historical_cov);

// Convenience: computes mu and Sigma from a historical returns matrix
// (T x N, one row per period), then the turbulence index of `latest_returns`
// against that history. Matches how the index is used in practice: mu/Sigma
// come from a trailing estimation window, evaluated against the newest bar.
double turbulence_index_from_history(const Eigen::MatrixXd& historical_returns,
                                     const Eigen::VectorXd& latest_returns);

// Percentile-based crash trigger: true iff `turbulence_t` exceeds the
// given percentile (default: 0.99, matching common turbulence-overlay
// practice) of `historical_turbulence_series`. This is the "halt new
// purchases" condition the source paper's ensemble strategy uses during
// the 2020 crash.
bool is_turbulent(double turbulence_t, const std::vector<double>& historical_turbulence_series,
                   double percentile = 0.99);

}  // namespace bazaartalks::quant_core
