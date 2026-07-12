#include "bazaartalks/linalg/regression.hpp"

#include <algorithm>
#include <limits>

namespace bazaartalks::linalg {

namespace {
// numpy's default rcond for lstsq/pinv (rcond=None resolves to this since
// numpy >= 1.14): machine epsilon * max(rows, cols).
double numpy_rcond(const Matrix& A) {
  return std::numeric_limits<double>::epsilon() *
         static_cast<double>(std::max(A.rows(), A.cols()));
}
}  // namespace

Vector ols(const Matrix& X, const Vector& y) {
  Eigen::BDCSVD<Matrix, Eigen::ComputeThinU | Eigen::ComputeThinV> svd(X);
  svd.setThreshold(numpy_rcond(X));
  return svd.solve(y);
}

Matrix pinv(const Matrix& A) {
  Eigen::BDCSVD<Matrix, Eigen::ComputeThinU | Eigen::ComputeThinV> svd(A);
  svd.setThreshold(numpy_rcond(A));
  const auto& singular_values = svd.singularValues();
  double tol = svd.threshold() * static_cast<double>(std::max(A.rows(), A.cols())) *
               (singular_values.size() > 0 ? singular_values(0) : 0.0);
  Vector inv_sv(singular_values.size());
  for (Eigen::Index i = 0; i < singular_values.size(); ++i) {
    inv_sv(i) = singular_values(i) > tol ? 1.0 / singular_values(i) : 0.0;
  }
  return svd.matrixV() * inv_sv.asDiagonal() * svd.matrixU().transpose();
}

Vector ridge(const Matrix& X, const Vector& y, double alpha) {
  Matrix XtX = X.transpose() * X;
  XtX += alpha * Matrix::Identity(XtX.rows(), XtX.cols());
  Vector Xty = X.transpose() * y;
  return XtX.ldlt().solve(Xty);
}

RidgeResult ridge_with_intercept(const Matrix& X, const Vector& y, double alpha) {
  Vector x_mean = X.colwise().mean();
  double y_mean = y.mean();
  Matrix Xc = X.rowwise() - x_mean.transpose();
  Vector yc = y.array() - y_mean;

  Vector beta = ridge(Xc, yc, alpha);
  double intercept = y_mean - x_mean.dot(beta);
  return RidgeResult{beta, intercept};
}

OlsResult ols_with_stats(const Matrix& X, const Vector& y) {
  Matrix Xd(X.rows(), X.cols() + 1);
  Xd.col(0).setOnes();
  Xd.rightCols(X.cols()) = X;

  Vector beta = ols(Xd, y);
  Vector resid = y - Xd * beta;

  double dof = static_cast<double>(Xd.rows() - Xd.cols());
  double sigma2 = resid.dot(resid) / dof;
  Matrix XtX_inv = (Xd.transpose() * Xd).inverse();
  Matrix cov = sigma2 * XtX_inv;

  Vector se(beta.size());
  Vector tvals(beta.size());
  for (Eigen::Index i = 0; i < beta.size(); ++i) {
    se(i) = std::sqrt(cov(i, i));
    tvals(i) = beta(i) / se(i);
  }

  double y_mean = y.mean();
  double ss_res = resid.dot(resid);
  double ss_tot = (y.array() - y_mean).square().sum();
  double r2 = 1.0 - ss_res / ss_tot;

  return OlsResult{beta, se, tvals, r2};
}

}  // namespace bazaartalks::linalg
