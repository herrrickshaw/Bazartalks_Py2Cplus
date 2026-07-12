#include "bazaartalks/quant_core/benchmark.hpp"

#include <cmath>
#include <limits>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
bool finite(double v) { return std::isfinite(v); }

double mean_of(const std::vector<double>& v) {
  double s = 0.0;
  for (double x : v) s += x;
  return s / static_cast<double>(v.size());
}

// ddof=1 sample std, matching pandas Series.std().
double sample_stddev(const std::vector<double>& v) {
  if (v.size() < 2) return 0.0;
  double m = mean_of(v);
  double sq = 0.0;
  for (double x : v) sq += (x - m) * (x - m);
  return std::sqrt(sq / static_cast<double>(v.size() - 1));
}
}  // namespace

CarhartResult carhart_alpha(const std::vector<double>& port_excess,
                            const std::vector<std::string>& factor_names,
                            const std::vector<std::vector<double>>& factor_returns) {
  std::size_t total = port_excess.size();
  std::size_t n_factors = factor_names.size();

  std::vector<std::size_t> valid_rows;
  for (std::size_t i = 0; i < total; ++i) {
    bool row_ok = finite(port_excess[i]);
    for (std::size_t f = 0; row_ok && f < n_factors; ++f) row_ok = finite(factor_returns[f][i]);
    if (row_ok) valid_rows.push_back(i);
  }

  std::size_t n = valid_rows.size();
  if (n < 30) {
    return CarhartResult{n, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {}, std::nullopt};
  }

  linalg::Matrix X(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n_factors));
  linalg::Vector y(static_cast<Eigen::Index>(n));
  for (std::size_t r = 0; r < n; ++r) {
    y(static_cast<Eigen::Index>(r)) = port_excess[valid_rows[r]];
    for (std::size_t f = 0; f < n_factors; ++f) {
      X(static_cast<Eigen::Index>(r), static_cast<Eigen::Index>(f)) = factor_returns[f][valid_rows[r]];
    }
  }

  auto result = linalg::ols_with_stats(X, y);
  double a_daily = result.beta(0);
  double a_t = result.t_stats(0);

  std::vector<FactorLoading> loadings;
  for (std::size_t f = 0; f < n_factors; ++f) {
    loadings.push_back(FactorLoading{factor_names[f], result.beta(static_cast<Eigen::Index>(f + 1)),
                                     result.t_stats(static_cast<Eigen::Index>(f + 1))});
  }

  return CarhartResult{
      n,
      a_daily,
      a_t,
      std::round(a_daily * 21.0 * 100.0 * 1000.0) / 1000.0,
      n >= 400,
      loadings,
      result.r_squared,
  };
}

std::vector<FactorPremium> factor_premia(const std::vector<std::string>& factor_names,
                                         const std::vector<std::vector<double>>& factor_series) {
  std::vector<FactorPremium> out;
  for (std::size_t f = 0; f < factor_names.size(); ++f) {
    std::vector<double> s;
    for (double v : factor_series[f])
      if (finite(v)) s.push_back(v);

    double mu = mean_of(s) * 252.0;
    double vol = sample_stddev(s) * std::sqrt(252.0);
    double sharpe = vol != 0.0 ? mu / vol : kNaN;

    out.push_back(FactorPremium{
        factor_names[f],
        std::round(mu * 100.0 * 100.0) / 100.0,
        std::round(vol * 100.0 * 100.0) / 100.0,
        std::isnan(sharpe) ? kNaN : std::round(sharpe * 100.0) / 100.0,
        s.size(),
    });
  }
  return out;
}

}  // namespace bazaartalks::quant_core
