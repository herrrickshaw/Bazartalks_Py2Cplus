#include "bazaartalks/quant_core/pit_backtest.hpp"

#include "bazaartalks/calendar/trading_calendar.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace bazaartalks::quant_core {

bool darvas_breakout(const std::vector<double>& close, std::size_t loc, std::size_t lookback) {
  if (loc < lookback) return false;
  double m = 0.0;
  bool any = false;
  // Python: `prior = close.iloc[loc-BREAKOUT_LB:loc]` -- EXCLUDES close[loc]
  // itself, and `.max()` skips NaN (a NaN prior slice -> NaN.max() -> the
  // subsequent `>=` comparison is False, matching "no finite prior -> not
  // a breakout" below).
  for (std::size_t j = loc - lookback; j < loc; ++j) {
    if (std::isfinite(close[j])) {
      if (!any || close[j] > m) m = close[j];
      any = true;
    }
  }
  if (!any) return false;
  return close[loc] >= m;
}

BacktestMetrics compute_metrics(const std::vector<double>& monthly_returns_pct) {
  BacktestMetrics out;
  std::size_t n = monthly_returns_pct.size();
  if (n == 0) return out;  // matches Python's bare {"n_months": 0}

  std::vector<double> r(n);
  for (std::size_t i = 0; i < n; ++i) r[i] = monthly_returns_pct[i] / 100.0;

  std::vector<double> eq(n);
  double running = 1.0;
  for (std::size_t i = 0; i < n; ++i) {
    running *= (1.0 + r[i]);
    eq[i] = running;
  }

  double mean = 0.0;
  for (double v : r) mean += v;
  mean /= static_cast<double>(n);
  // np.array(rets).std(): numpy's default ddof=0 (POPULATION std), not
  // pandas Series.std()'s ddof=1 default -- `rets` here is a plain numpy
  // array, not a pandas object, so this is genuinely population variance.
  double var = 0.0;
  for (double v : r) var += (v - mean) * (v - mean);
  var /= static_cast<double>(n);
  double sd = std::sqrt(var);
  double sharpe = sd > 0.0 ? (mean / sd * std::sqrt(12.0)) : 0.0;

  double running_max = eq[0];
  double min_dd = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    running_max = std::max(running_max, eq[i]);
    double dd = (eq[i] - running_max) / running_max;
    if (i == 0 || dd < min_dd) min_dd = dd;
  }

  int hit = 0;
  for (double v : r) if (v > 0.0) ++hit;

  double ann = (std::pow(eq[n - 1], 12.0 / static_cast<double>(n)) - 1.0) * 100.0;

  auto round_n = [](double x, int decimals) {
    double scale = std::pow(10.0, decimals);
    return std::round(x * scale) / scale;
  };

  out.n_months = static_cast<int>(n);
  out.avg_month_pct = round_n(mean * 100.0, 3);
  out.ann_return_pct = round_n(ann, 2);
  out.hit_rate_pct = round_n(static_cast<double>(hit) / static_cast<double>(n) * 100.0, 1);
  out.sharpe = round_n(sharpe, 2);
  out.max_dd_pct = round_n(min_dd * 100.0, 1);
  return out;
}

std::vector<PeriodReturn> period_returns(const std::vector<TickerHistory>& tickers,
                                          date::sys_days t0, date::sys_days t1,
                                          std::size_t darvas_lookback) {
  using bazaartalks::calendar::ffill_index;

  std::vector<PeriodReturn> out;
  for (const auto& t : tickers) {
    auto idx0 = ffill_index(t.dates, t0);
    auto idx1 = ffill_index(t.dates, t1);
    if (!idx0.has_value() || !idx1.has_value()) continue;
    if (*idx0 >= *idx1) continue;  // e0 >= e1

    double fwd = (t.close[*idx1] / t.close[*idx0] - 1.0) * 100.0;
    if (!std::isfinite(fwd)) continue;

    bool breakout = darvas_breakout(t.close, *idx0, darvas_lookback);
    out.push_back(PeriodReturn{t.ticker, fwd, breakout});
  }
  return out;
}

std::vector<date::sys_days> monthly_rebalance_dates(const std::vector<TickerHistory>& tickers,
                                                     std::size_t min_lookback) {
  std::set<date::sys_days> union_set;
  for (const auto& t : tickers) {
    for (auto d : t.dates) union_set.insert(d);
  }
  std::vector<date::sys_days> all_idx(union_set.begin(), union_set.end());
  if (all_idx.size() <= min_lookback + 1) return {};
  date::sys_days cutoff = all_idx[min_lookback + 1];

  // First trading day present in the union for each (year, month).
  std::map<std::pair<int, unsigned>, date::sys_days> first_of_month;
  for (date::sys_days d : all_idx) {
    date::year_month_day ymd{d};
    auto key = std::make_pair(static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()));
    auto it = first_of_month.find(key);
    if (it == first_of_month.end() || d < it->second) first_of_month[key] = d;
  }

  std::vector<date::sys_days> out;
  for (const auto& [key, d] : first_of_month) {
    if (d >= cutoff) out.push_back(d);
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace bazaartalks::quant_core
