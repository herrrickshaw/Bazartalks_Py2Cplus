#include "bazaartalks/stats/cross_sectional.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace bazaartalks::stats {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

bool finite(double v) { return std::isfinite(v); }

// coerce +-inf -> NaN, matching zscore()'s `.replace([np.inf,-np.inf], np.nan)`
double sanitize(double v) { return std::isinf(v) ? kNaN : v; }
}  // namespace

double mean_skipnan(const std::vector<double>& values) {
  double sum = 0.0;
  std::size_t n = 0;
  for (double v : values) {
    if (finite(v)) {
      sum += v;
      ++n;
    }
  }
  return n > 0 ? sum / static_cast<double>(n) : kNaN;
}

double population_stddev(const std::vector<double>& values) {
  double m = mean_skipnan(values);
  if (std::isnan(m)) return kNaN;
  double sq = 0.0;
  std::size_t n = 0;
  for (double v : values) {
    if (finite(v)) {
      double d = v - m;
      sq += d * d;
      ++n;
    }
  }
  return n > 0 ? std::sqrt(sq / static_cast<double>(n)) : kNaN;
}

std::vector<double> zscore(const std::vector<double>& values) {
  std::vector<double> sanitized;
  sanitized.reserve(values.size());
  for (double v : values) sanitized.push_back(sanitize(v));

  double sd = population_stddev(sanitized);
  if (sd == 0.0 || std::isnan(sd)) {
    // Python: pd.Series(0.0, index=s.index) -- the WHOLE output becomes
    // 0.0, including originally-NaN entries. Not a bug; replicate exactly.
    return std::vector<double>(values.size(), 0.0);
  }
  double m = mean_skipnan(sanitized);
  std::vector<double> out(sanitized.size());
  for (std::size_t i = 0; i < sanitized.size(); ++i) {
    out[i] = finite(sanitized[i]) ? (sanitized[i] - m) / sd : kNaN;
  }
  return out;
}

double pearson_corr(const std::vector<double>& x, const std::vector<double>& y) {
  std::size_t n = std::min(x.size(), y.size());
  double sx = 0, sy = 0;
  std::size_t count = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (finite(x[i]) && finite(y[i])) {
      sx += x[i];
      sy += y[i];
      ++count;
    }
  }
  if (count < 2) return kNaN;
  double mx = sx / static_cast<double>(count);
  double my = sy / static_cast<double>(count);

  double cov = 0, vx = 0, vy = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (finite(x[i]) && finite(y[i])) {
      double dx = x[i] - mx;
      double dy = y[i] - my;
      cov += dx * dy;
      vx += dx * dx;
      vy += dy * dy;
    }
  }
  if (vx == 0.0 || vy == 0.0) return kNaN;
  return cov / std::sqrt(vx * vy);
}

double information_coefficient(const std::vector<double>& signal,
                                const std::vector<double>& fwd_ret) {
  std::size_t n = std::min(signal.size(), fwd_ret.size());
  std::vector<double> s, r;
  s.reserve(n);
  r.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (finite(signal[i]) && finite(fwd_ret[i])) {
      s.push_back(signal[i]);
      r.push_back(fwd_ret[i]);
    }
  }
  if (s.size() < 10) return kNaN;
  if (population_stddev(s) == 0.0 || population_stddev(r) == 0.0) return kNaN;
  return pearson_corr(s, r);
}

std::vector<double> rank_average(const std::vector<double>& values) {
  std::size_t n = values.size();
  std::vector<std::size_t> order(n);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return values[a] < values[b]; });

  std::vector<double> ranks(n);
  std::size_t i = 0;
  while (i < n) {
    std::size_t j = i;
    while (j + 1 < n && values[order[j + 1]] == values[order[i]]) ++j;
    // average of the 1-indexed positions i+1..j+1
    double avg_rank = (static_cast<double>(i + 1) + static_cast<double>(j + 1)) / 2.0;
    for (std::size_t k = i; k <= j; ++k) ranks[order[k]] = avg_rank;
    i = j + 1;
  }
  return ranks;
}

double monotonicity(const std::vector<double>& values) {
  if (values.size() < 3) return kNaN;
  std::vector<double> ranks = rank_average(values);
  std::vector<double> ideal(values.size());
  for (std::size_t i = 0; i < ideal.size(); ++i) ideal[i] = static_cast<double>(i + 1);
  return pearson_corr(ranks, ideal);
}

double trend_corr(const std::vector<double>& values) {
  std::vector<double> finite_vals;
  finite_vals.reserve(values.size());
  for (double v : values)
    if (finite(v)) finite_vals.push_back(v);

  if (finite_vals.size() < 3) return kNaN;
  if (population_stddev(finite_vals) == 0.0) return kNaN;

  std::vector<double> idx(finite_vals.size());
  for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<double>(i);
  return pearson_corr(finite_vals, idx);
}

}  // namespace bazaartalks::stats
