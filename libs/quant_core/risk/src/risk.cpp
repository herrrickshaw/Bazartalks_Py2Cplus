#include "bazaartalks/quant_core/risk.hpp"

#include <algorithm>
#include <cmath>

namespace bazaartalks::quant_core {

namespace {
double mean(const std::vector<double>& v) {
  double s = 0.0;
  for (double x : v) s += x;
  return v.empty() ? 0.0 : s / static_cast<double>(v.size());
}

// ddof=1 sample standard deviation, matching numpy's `.std(ddof=1)`.
double stddev_ddof1(const std::vector<double>& v) {
  if (v.size() < 2) return 0.0;
  double m = mean(v);
  double sq = 0.0;
  for (double x : v) sq += (x - m) * (x - m);
  return std::sqrt(sq / static_cast<double>(v.size() - 1));
}
}  // namespace

double quantile_linear(std::vector<double> values, double q) {
  std::sort(values.begin(), values.end());
  std::size_t n = values.size();
  if (n == 0) return 0.0;
  if (n == 1) return values[0];
  double pos = q * static_cast<double>(n - 1);
  std::size_t lo = static_cast<std::size_t>(std::floor(pos));
  std::size_t hi = static_cast<std::size_t>(std::ceil(pos));
  double frac = pos - static_cast<double>(lo);
  return values[lo] + frac * (values[hi] - values[lo]);
}

std::vector<double> equity_curve(const std::vector<double>& returns) {
  std::vector<double> out(returns.size());
  double cum = 1.0;
  for (std::size_t i = 0; i < returns.size(); ++i) {
    cum *= (1.0 + returns[i]);
    out[i] = cum;
  }
  return out;
}

double max_drawdown(const std::vector<double>& returns) {
  if (returns.empty()) return 0.0;
  auto eq = equity_curve(returns);
  double peak = eq[0];
  double worst = 0.0;
  for (double e : eq) {
    peak = std::max(peak, e);
    worst = std::min(worst, (e - peak) / peak);
  }
  return worst;
}

double hist_var(const std::vector<double>& returns, double alpha) {
  if (returns.empty()) return 0.0;
  return -quantile_linear(returns, alpha);
}

double cvar(const std::vector<double>& returns, double alpha) {
  if (returns.empty()) return 0.0;
  double thr = quantile_linear(returns, alpha);
  std::vector<double> tail;
  for (double r : returns)
    if (r <= thr) tail.push_back(r);
  if (tail.empty()) return -thr;
  return -mean(tail);
}

double ann_vol(const std::vector<double>& returns, int periods) {
  if (returns.size() <= 1) return 0.0;
  return stddev_ddof1(returns) * std::sqrt(static_cast<double>(periods));
}

double ann_return(const std::vector<double>& returns, int periods) {
  if (returns.empty()) return 0.0;
  auto eq = equity_curve(returns);
  return std::pow(eq.back(), static_cast<double>(periods) / static_cast<double>(returns.size())) -
         1.0;
}

double sharpe(const std::vector<double>& returns, double rf, int periods) {
  std::vector<double> ex(returns.size());
  double rf_per_period = rf / static_cast<double>(periods);
  for (std::size_t i = 0; i < returns.size(); ++i) ex[i] = returns[i] - rf_per_period;
  double sd = stddev_ddof1(ex);
  if (sd <= 0.0) return 0.0;
  return mean(ex) / sd * std::sqrt(static_cast<double>(periods));
}

double sortino(const std::vector<double>& returns, double rf, int periods) {
  std::vector<double> ex(returns.size());
  double rf_per_period = rf / static_cast<double>(periods);
  for (std::size_t i = 0; i < returns.size(); ++i) ex[i] = returns[i] - rf_per_period;

  std::vector<double> downside;
  for (double x : ex)
    if (x < 0.0) downside.push_back(x);
  double dd = downside.size() > 1 ? stddev_ddof1(downside) : 0.0;
  if (dd <= 0.0) return 0.0;
  return mean(ex) / dd * std::sqrt(static_cast<double>(periods));
}

RegimeFlag regime_flag(const std::vector<double>& returns, std::size_t window, int periods) {
  if (returns.size() < window + 1) {
    return RegimeFlag{"unknown", std::nullopt, std::nullopt};
  }
  std::vector<double> recent(returns.end() - static_cast<std::ptrdiff_t>(window), returns.end());
  double recent_vol = stddev_ddof1(recent) * std::sqrt(static_cast<double>(periods));
  double full_vol = stddev_ddof1(returns) * std::sqrt(static_cast<double>(periods));
  double ratio = full_vol > 0.0 ? recent_vol / full_vol : 1.0;

  auto eq = equity_curve(returns);
  double running_max = eq[0];
  for (double e : eq) running_max = std::max(running_max, e);
  bool in_dd = eq.back() < running_max * 0.98;

  std::string regime;
  if (ratio > 1.3 && in_dd) {
    regime = "risk_off";
  } else if (ratio > 1.15 || in_dd) {
    regime = "caution";
  } else {
    regime = "risk_on";
  }
  return RegimeFlag{regime, ratio, in_dd};
}

}  // namespace bazaartalks::quant_core
