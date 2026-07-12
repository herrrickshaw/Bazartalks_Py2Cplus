#include "bazaartalks/quant_core/portfolio.hpp"

#include "bazaartalks/linalg/regression.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

namespace bazaartalks::quant_core {

Vector min_variance_weights(const Matrix& cov) {
  Matrix inv = bazaartalks::linalg::pinv(cov);
  Vector one = Vector::Ones(cov.rows());
  Vector w = inv * one;
  return w / w.sum();
}

Vector max_sharpe_weights(const Vector& mu, const Matrix& cov) {
  Matrix inv = bazaartalks::linalg::pinv(cov);
  Vector w = inv * mu;
  double s = w.sum();
  if (s != 0.0) return w / s;
  return Vector::Ones(mu.size()) / static_cast<double>(mu.size());
}

Vector long_only(const Vector& w_in) {
  Vector w = w_in.cwiseMax(0.0);
  double s = w.sum();
  if (s > 0.0) return w / s;
  return Vector::Ones(w.size()) / static_cast<double>(w.size());
}

Vector cap_weights(const Vector& w_in, double cap, int iters) {
  Vector w = w_in;
  Eigen::Index n = w.size();
  if (cap * static_cast<double>(n) < 1.0 - 1e-9) {
    return Vector::Ones(n) / static_cast<double>(n);
  }
  for (int it = 0; it < iters; ++it) {
    std::vector<bool> over(n);
    bool any_over = false;
    double excess = 0.0;
    for (Eigen::Index i = 0; i < n; ++i) {
      over[i] = w(i) > cap + 1e-12;
      if (over[i]) {
        any_over = true;
        excess += w(i) - cap;
      }
    }
    if (!any_over) break;
    for (Eigen::Index i = 0; i < n; ++i)
      if (over[i]) w(i) = cap;

    double free_sum = 0.0;
    for (Eigen::Index i = 0; i < n; ++i)
      if (!over[i] && w(i) > 0.0) free_sum += w(i);

    if (free_sum <= 0.0) {
      w.setConstant(cap);
      break;
    }
    for (Eigen::Index i = 0; i < n; ++i) {
      if (!over[i] && w(i) > 0.0) w(i) += excess * w(i) / free_sum;
    }
  }
  return w / w.sum();
}

Vector apply_sector_cap(const Vector& w_in, const std::vector<std::string>& sectors, double cap,
                        int iters) {
  Vector w = w_in;
  Eigen::Index n = w.size();

  std::vector<std::string> uniq;
  {
    std::set<std::string> seen;
    for (const auto& s : sectors) {
      if (seen.insert(s).second) uniq.push_back(s);
    }
  }
  if (cap * static_cast<double>(uniq.size()) < 1.0 - 1e-9) {
    return w / w.sum();  // infeasible -- leave as-is (renormalised), matching Python
  }

  for (int it = 0; it < iters; ++it) {
    std::unordered_map<std::string, double> totals;
    for (const auto& s : uniq) totals[s] = 0.0;
    for (Eigen::Index i = 0; i < n; ++i) totals[sectors[i]] += w(i);

    std::vector<std::string> over;
    for (const auto& s : uniq)
      if (totals[s] > cap + 1e-12) over.push_back(s);
    if (over.empty()) break;

    std::set<std::string> over_set(over.begin(), over.end());
    double excess = 0.0;
    for (const auto& s : over) {
      double sector_sum = totals[s];
      excess += sector_sum - cap;
      double scale = cap / sector_sum;
      for (Eigen::Index i = 0; i < n; ++i)
        if (sectors[i] == s) w(i) *= scale;
    }

    double free_sum = 0.0;
    for (Eigen::Index i = 0; i < n; ++i) {
      if (!over_set.count(sectors[i]) && w(i) > 0.0) free_sum += w(i);
    }
    if (free_sum <= 0.0) break;
    for (Eigen::Index i = 0; i < n; ++i) {
      if (!over_set.count(sectors[i]) && w(i) > 0.0) w(i) += excess * w(i) / free_sum;
    }
  }
  return w / w.sum();
}

double turnover(const Vector& w_new, const Vector& w_old) {
  return 0.5 * (w_new - w_old).cwiseAbs().sum();
}

Vector blend_to_turnover(const Vector& w_new, const Vector& w_old, double max_turnover) {
  double t = turnover(w_new, w_old);
  if (t <= max_turnover || t == 0.0) return w_new;
  double lambda = max_turnover / t;
  Vector w = w_old + lambda * (w_new - w_old);
  return w / w.sum();
}

}  // namespace bazaartalks::quant_core
