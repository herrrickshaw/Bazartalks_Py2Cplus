#include "bazaartalks/quant_core/crowding.hpp"

#include <cmath>
#include <limits>

#include "bazaartalks/stats/cross_sectional.hpp"

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
bool finite(double v) { return std::isfinite(v); }
}  // namespace

double corr_to_market(const std::vector<double>& stock_ret, const std::vector<double>& market_ret) {
  std::size_t n = std::min(stock_ret.size(), market_ret.size());
  std::vector<double> a, b;
  a.reserve(n);
  b.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (finite(stock_ret[i]) && finite(market_ret[i])) {
      a.push_back(stock_ret[i]);
      b.push_back(market_ret[i]);
    }
  }
  if (a.size() < 20) return kNaN;
  if (bazaartalks::stats::population_stddev(a) == 0.0 ||
      bazaartalks::stats::population_stddev(b) == 0.0) {
    return kNaN;
  }
  return bazaartalks::stats::pearson_corr(a, b);
}

double rel_strength(const std::vector<double>& prices, std::size_t lookback) {
  std::vector<double> c;
  c.reserve(prices.size());
  for (double p : prices)
    if (finite(p)) c.push_back(p);

  if (c.size() < lookback + 1) return kNaN;
  double denom = c[c.size() - lookback];  // c.iloc[-lookback]
  if (denom <= 0.0) return kNaN;
  return c.back() / denom - 1.0;
}

std::vector<double> crowding_score(const std::vector<CrowdingFeatures>& features) {
  std::size_t n = features.size();
  std::vector<double> corr(n), rs(n);
  for (std::size_t i = 0; i < n; ++i) {
    corr[i] = features[i].corr_mkt;
    rs[i] = features[i].rel_strength;
  }
  auto corr_rank_pct = bazaartalks::stats::rank_average(corr);
  auto rs_rank_pct = bazaartalks::stats::rank_average(rs);
  std::vector<double> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    double corr_r = corr_rank_pct[i] / static_cast<double>(n);
    double rs_r = rs_rank_pct[i] / static_cast<double>(n);
    double score = 100.0 * (0.65 * corr_r + 0.35 * rs_r);
    out[i] = std::round(score * 10.0) / 10.0;  // .round(1), matching Python
  }
  return out;
}

}  // namespace bazaartalks::quant_core
