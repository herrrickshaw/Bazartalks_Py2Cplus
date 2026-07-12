#include "bazaartalks/quant_core/hft_selection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "bazaartalks/stats/cross_sectional.hpp"

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
bool finite(double v) { return std::isfinite(v); }

double round3(double v) {
  if (std::isnan(v)) return kNaN;
  return std::round(v * 1000.0) / 1000.0;
}
}  // namespace

std::vector<double> daily_range_pct(const std::vector<double>& high,
                                     const std::vector<double>& low,
                                     const std::vector<double>& close) {
  std::size_t n = std::min({high.size(), low.size(), close.size()});
  std::vector<double> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = close[i] > 0.0 ? (high[i] - low[i]) / close[i] : kNaN;
  }
  return out;
}

double avg_range(const std::vector<double>& high, const std::vector<double>& low,
                  const std::vector<double>& close) {
  auto r = daily_range_pct(high, low, close);
  double sum = 0.0;
  std::size_t count = 0;
  for (double x : r)
    if (finite(x)) {
      sum += x;
      ++count;
    }
  return count > 0 ? sum / static_cast<double>(count) : kNaN;
}

double range_stability(const std::vector<double>& high, const std::vector<double>& low,
                        const std::vector<double>& close) {
  auto r = daily_range_pct(high, low, close);
  double m = avg_range(high, low, close);
  if (std::isnan(m)) return kNaN;
  double sq = 0.0;
  std::size_t count = 0;
  for (double x : r)
    if (finite(x)) {
      sq += (x - m) * (x - m);
      ++count;
    }
  return count > 0 ? std::sqrt(sq / static_cast<double>(count)) : kNaN;  // population std (nanstd)
}

double corwin_schultz_spread(const std::vector<double>& high, const std::vector<double>& low) {
  std::size_t n = std::min(high.size(), low.size());
  if (n < 2) return kNaN;
  constexpr double k = 3.0 - 2.0 * 1.4142135623730951;  // 3 - 2*sqrt(2)

  std::vector<double> spreads;
  for (std::size_t t = 0; t + 1 < n; ++t) {
    if (low[t] <= 0.0 || low[t + 1] <= 0.0) continue;
    double beta = std::pow(std::log(high[t] / low[t]), 2) + std::pow(std::log(high[t + 1] / low[t + 1]), 2);
    double hi2 = std::max(high[t], high[t + 1]);
    double lo2 = std::min(low[t], low[t + 1]);
    if (lo2 <= 0.0) continue;
    double gamma = std::pow(std::log(hi2 / lo2), 2);
    double alpha = (std::sqrt(2.0 * beta) - std::sqrt(beta)) / k - std::sqrt(gamma / k);
    double s = 2.0 * (std::exp(alpha) - 1.0) / (1.0 + std::exp(alpha));
    spreads.push_back(std::max(s, 0.0));
  }
  if (spreads.empty()) return kNaN;
  double sum = 0.0;
  for (double s : spreads) sum += s;
  return sum / static_cast<double>(spreads.size());
}

double efficiency_ratio(const std::vector<double>& close) {
  if (close.size() < 2) return kNaN;
  double travel = 0.0;
  for (std::size_t i = 1; i < close.size(); ++i) travel += std::abs(close[i] - close[i - 1]);
  if (travel <= 0.0) return kNaN;
  return std::abs(close.back() - close.front()) / travel;
}

double lag1_autocorr(const std::vector<double>& x) {
  std::vector<double> a;
  a.reserve(x.size());
  for (double v : x)
    if (finite(v)) a.push_back(v);
  if (a.size() < 3) return kNaN;

  double mean = 0.0;
  for (double v : a) mean += v;
  mean /= static_cast<double>(a.size());
  double var = 0.0;
  for (double v : a) var += (v - mean) * (v - mean);
  if (var == 0.0) return kNaN;

  std::vector<double> lagged(a.begin(), a.end() - 1);
  std::vector<double> shifted(a.begin() + 1, a.end());
  return bazaartalks::stats::pearson_corr(lagged, shifted);
}

double ou_half_life(const std::vector<double>& close) {
  std::vector<double> p;
  p.reserve(close.size());
  for (double v : close)
    if (finite(v)) p.push_back(v);
  if (p.size() < 4) return kNaN;

  std::vector<double> lag(p.begin(), p.end() - 1);
  std::vector<double> dp(p.size() - 1);
  for (std::size_t i = 0; i < dp.size(); ++i) dp[i] = p[i + 1] - p[i];

  // slope of a degree-1 polyfit(dp ~ lag): b = cov(lag,dp)/var(lag)
  double lag_mean = 0.0;
  for (double v : lag) lag_mean += v;
  lag_mean /= static_cast<double>(lag.size());
  double dp_mean = 0.0;
  for (double v : dp) dp_mean += v;
  dp_mean /= static_cast<double>(dp.size());

  double cov = 0.0, var = 0.0;
  for (std::size_t i = 0; i < lag.size(); ++i) {
    cov += (lag[i] - lag_mean) * (dp[i] - dp_mean);
    var += (lag[i] - lag_mean) * (lag[i] - lag_mean);
  }
  double b = cov / var;

  if (b >= -1e-9 || (1.0 + b) <= 0.0) return kInf;
  return -std::log(2.0) / std::log(1.0 + b);
}

std::vector<ArchetypeScores> archetype_scores(const std::vector<HftFeatures>& features) {
  std::size_t n = features.size();
  std::vector<double> avg_r(n), range_stab(n), eff(n), ret_ac(n), vol_ac(n), hl(n);
  std::vector<double> peer_corr(n), peer_dev_abs(n);
  bool any_peer_corr = false;

  for (std::size_t i = 0; i < n; ++i) {
    const auto& f = features[i];
    avg_r[i] = f.avg_range_pct;
    range_stab[i] = f.range_stability;
    eff[i] = f.eff_ratio;
    ret_ac[i] = f.ret_autocorr;
    vol_ac[i] = f.vol_autocorr;
    // `.replace([inf,-inf], NaN).clip(0, 60)` -- clip only applies to
    // already-finite values; a non-finite half_life becomes NaN outright.
    hl[i] = std::isfinite(f.half_life) ? std::clamp(f.half_life, 0.0, 60.0) : kNaN;
    if (f.peer_corr) {
      peer_corr[i] = *f.peer_corr;
      any_peer_corr = true;
    } else {
      peer_corr[i] = kNaN;
    }
    peer_dev_abs[i] = f.peer_dev ? std::abs(*f.peer_dev) : kNaN;
  }

  auto z_avg_r = bazaartalks::stats::zscore(avg_r);
  auto z_range_stab = bazaartalks::stats::zscore(range_stab);
  auto z_eff = bazaartalks::stats::zscore(eff);
  auto z_ret_ac = bazaartalks::stats::zscore(ret_ac);
  auto z_vol_ac = bazaartalks::stats::zscore(vol_ac);
  auto z_hl = bazaartalks::stats::zscore(hl);

  std::vector<double> z_peer_corr, z_peer_dev;
  if (any_peer_corr) {
    z_peer_corr = bazaartalks::stats::zscore(peer_corr);
    z_peer_dev = bazaartalks::stats::zscore(peer_dev_abs);
  }

  std::vector<ArchetypeScores> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    out[i].market_making = round3(-z_avg_r[i] - z_range_stab[i] - z_eff[i]);
    out[i].stat_arb = round3(-z_ret_ac[i] - z_eff[i] - z_hl[i]);
    out[i].latency = round3(z_eff[i] + z_vol_ac[i] + z_ret_ac[i]);
    if (any_peer_corr) {
      double v = round3(z_peer_corr[i] + z_peer_dev[i]);
      out[i].etf_arb = std::isnan(v) ? std::nullopt : std::optional<double>(v);
    } else {
      out[i].etf_arb = std::nullopt;
    }
  }
  return out;
}

}  // namespace bazaartalks::quant_core
