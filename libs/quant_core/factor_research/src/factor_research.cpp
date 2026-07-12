#include "bazaartalks/quant_core/factor_research.hpp"

#include <cmath>

namespace bazaartalks::quant_core {

namespace {
PortfolioStats score(const Vector& w, const Matrix& cov, const Vector& fwd_ret_pct) {
  double ret = w.dot(fwd_ret_pct);
  double vol = std::sqrt(w.dot(cov * w)) * 100.0;
  double ret_over_vol = vol != 0.0 ? ret / vol : 0.0;
  return PortfolioStats{ret, vol, ret_over_vol};
}
}  // namespace

MarkowitzResult markowitz_portfolios(const Vector& mu, const Matrix& cov,
                                      const Vector& fwd_ret_pct) {
  Eigen::Index n = mu.size();

  Vector concentrated = Vector::Zero(n);
  Eigen::Index best;
  mu.maxCoeff(&best);
  concentrated(best) = 1.0;

  Vector equal_weight = Vector::Ones(n) / static_cast<double>(n);
  Vector w_mv = min_variance_weights(cov);
  Vector w_ms = max_sharpe_weights(mu, cov);

  return MarkowitzResult{
      score(concentrated, cov, fwd_ret_pct),
      score(equal_weight, cov, fwd_ret_pct),
      score(w_mv, cov, fwd_ret_pct),
      score(w_ms, cov, fwd_ret_pct),
  };
}

}  // namespace bazaartalks::quant_core
