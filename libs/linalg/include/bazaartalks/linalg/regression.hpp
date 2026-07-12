#pragma once
// Eigen-based linear algebra wrappers used by factor_research.py's
// hand-rolled OLS, portfolio.py's pinv-based mean-variance optimizers, and
// ml_signal_engine.py's closed-form Ridge regression.
//
// Rank-deficiency handling note (see the migration plan, Phase 1): numpy's
// lstsq/pinv use an SVD-based cutoff (`rcond`) to treat near-zero singular
// values as zero. We replicate that with Eigen's JacobiSVD/BDCSVD using the
// same default relative tolerance numpy uses
// (rcond = max(rows, cols) * epsilon of the largest singular value),
// rather than Eigen's own default tolerance, so near-singular covariance
// matrices in portfolio.py's optimizers behave the same way in both.

#include <Eigen/Dense>

namespace bazaartalks::linalg {

using Vector = Eigen::VectorXd;
using Matrix = Eigen::MatrixXd;

// Ordinary least squares via SVD (equivalent to numpy.linalg.lstsq(X, y)):
// solves min ||X*beta - y||^2. Returns beta.
Vector ols(const Matrix& X, const Vector& y);

// Moore-Penrose pseudo-inverse matching numpy.linalg.pinv's default rcond
// convention (see file-level note above).
Matrix pinv(const Matrix& A);

// Closed-form ridge regression: beta = (X^T X + alpha*I)^-1 X^T y.
// Matches ml_signal_engine.py's sklearn.linear_model.Ridge(alpha=...) for
// the no-intercept, pre-centered-feature case that module uses internally
// (features are z-score normalised before this is called).
Vector ridge(const Matrix& X, const Vector& y, double alpha);

// Ridge-regression standard errors (OLS-style, homoskedastic) are NOT
// computed here -- factor_research.py's hand-rolled OLS t-stats are a
// separate, explicit function since they're only needed for the OLS path,
// not ridge.
struct OlsResult {
  Vector beta;         // beta[0] is the intercept, beta[1..] match X's columns
  Vector std_errors;   // homoskedastic SEs, sqrt(diag(sigma^2 * inv(Xd^T Xd)))
  Vector t_stats;      // beta / std_errors
  double r_squared;
};

// Port of factor_research.py's ols(y, X, names): X does NOT include an
// intercept column -- like the Python original, this function prepends a
// leading ones-column internally (`np.column_stack([np.ones(len(X)), X])`)
// before solving, so beta/std_errors/t_stats have size X.cols()+1.
// Standard errors use plain `inv(Xd^T Xd)` (not pinv), matching Python's
// `np.linalg.inv` call in the same function -- this assumes X is full
// column rank, exactly as the Python original does.
OlsResult ols_with_stats(const Matrix& X, const Vector& y);

}  // namespace bazaartalks::linalg
