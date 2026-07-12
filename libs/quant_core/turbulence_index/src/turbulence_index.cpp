#include "bazaartalks/quant_core/turbulence_index.hpp"

#include "bazaartalks/linalg/regression.hpp"
#include "bazaartalks/quant_core/risk.hpp"  // reuse quantile_linear -- same numpy percentile formula

namespace bazaartalks::quant_core {

double turbulence_index(const Eigen::VectorXd& returns_t, const Eigen::VectorXd& historical_mean,
                        const Eigen::MatrixXd& historical_cov) {
  Eigen::VectorXd d = returns_t - historical_mean;
  Eigen::MatrixXd sigma_inv = linalg::pinv(historical_cov);
  return d.dot(sigma_inv * d);
}

double turbulence_index_from_history(const Eigen::MatrixXd& historical_returns,
                                     const Eigen::VectorXd& latest_returns) {
  Eigen::VectorXd mu = historical_returns.colwise().mean();
  Eigen::Index t = historical_returns.rows();
  Eigen::MatrixXd centered = historical_returns.rowwise() - mu.transpose();
  // np.cov(returns.T) default: ddof=1 (sample covariance).
  Eigen::MatrixXd sigma = (centered.transpose() * centered) / static_cast<double>(t - 1);
  return turbulence_index(latest_returns, mu, sigma);
}

bool is_turbulent(double turbulence_t, const std::vector<double>& historical_turbulence_series,
                   double percentile) {
  double threshold = quantile_linear(historical_turbulence_series, percentile);
  return turbulence_t > threshold;
}

}  // namespace bazaartalks::quant_core
