#include "bazaartalks/quant_core/dvm_global.hpp"

#include "bazaartalks/stats/technical_indicators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

std::size_t count_finite(const std::vector<double>& x) {
  std::size_t n = 0;
  for (double v : x) {
    if (std::isfinite(v)) ++n;
  }
  return n;
}

// numpy's np.minimum(100, np.maximum(0, x)): NaN propagates through both
// (unlike Python's builtin min()/max(), which silently drop a NaN operand
// because NaN comparisons are always False) -- see rolling_max/rolling_min
// for the same "IEEE comparison, not skip" convention used elsewhere.
double clip0_100(double x) {
  if (std::isnan(x)) return kNaN;
  return std::min(100.0, std::max(0.0, x));
}

// Same clip, but only upper-bounded at 100 -- sub-score 6
// (`np.minimum(100, 50 * VOLR.values)`) has no lower clip in the Python
// original; replicated as-is, not "fixed" to match the other five.
double clip_upper_100(double x) {
  if (std::isnan(x)) return kNaN;
  return std::min(100.0, x);
}

// pandas `.mean(axis=1)`'s default skipna=True over the 6 M sub-scores.
double mean_skip_nan(const double (&subs)[6]) {
  double sum = 0.0;
  int count = 0;
  for (double v : subs) {
    if (!std::isnan(v)) {
      sum += v;
      ++count;
    }
  }
  return count > 0 ? sum / static_cast<double>(count) : kNaN;
}

}  // namespace

std::vector<DvmGlobalMetrics> process_market(const std::string& market,
                                              const std::vector<TickerSeries>& tickers,
                                              std::size_t min_bars) {
  using namespace bazaartalks::stats;

  std::vector<DvmGlobalMetrics> out;
  if (tickers.empty()) return out;

  std::size_t n_dates = tickers.front().close.size();
  std::size_t n_tickers = tickers.size();

  // --- beta vs equal-weight market: needs every ticker's daily returns at
  // once, computed up front before the per-ticker snapshot loop below. ---
  std::vector<std::vector<double>> returns(n_tickers, std::vector<double>(n_dates, kNaN));
  for (std::size_t k = 0; k < n_tickers; ++k) {
    const auto& c = tickers[k].close;
    for (std::size_t i = 1; i < n_dates; ++i) {
      if (std::isfinite(c[i]) && std::isfinite(c[i - 1]) && c[i - 1] != 0.0) {
        returns[k][i] = c[i] / c[i - 1] - 1.0;
      }
    }
  }
  // m[i] = mean, across tickers, of each ticker's return on date i --
  // pandas' `R.mean(axis=1)`, skipping NaN (not-yet-listed tickers, or the
  // universal first-date NaN every pct_change column has).
  std::vector<double> mkt_mean_return(n_dates, kNaN);
  for (std::size_t i = 0; i < n_dates; ++i) {
    double sum = 0.0;
    int count = 0;
    for (std::size_t k = 0; k < n_tickers; ++k) {
      if (std::isfinite(returns[k][i])) {
        sum += returns[k][i];
        ++count;
      }
    }
    if (count > 0) mkt_mean_return[i] = sum / static_cast<double>(count);
  }

  std::size_t tail = std::min<std::size_t>(200, n_dates);
  std::size_t tail_start = n_dates - tail;

  double mean_m2 = kNaN, var_m2 = kNaN;
  {
    double sum = 0.0;
    int count = 0;
    for (std::size_t i = tail_start; i < n_dates; ++i) {
      if (std::isfinite(mkt_mean_return[i])) {
        sum += mkt_mean_return[i];
        ++count;
      }
    }
    if (count > 0) mean_m2 = sum / static_cast<double>(count);
    if (count >= 2) {
      double sq = 0.0;
      for (std::size_t i = tail_start; i < n_dates; ++i) {
        if (std::isfinite(mkt_mean_return[i])) {
          double d = mkt_mean_return[i] - mean_m2;
          sq += d * d;
        }
      }
      var_m2 = sq / static_cast<double>(count - 1);  // ddof=1, pandas' default
    }
  }
  // `m2.var() or np.nan`: Python's `or` only substitutes when the left
  // operand is falsy, i.e. exactly 0.0 -- NaN is truthy in Python (NaN!=0)
  // and passes straight through unchanged.
  double beta_denom = (var_m2 == 0.0) ? kNaN : var_m2;

  for (std::size_t k = 0; k < n_tickers; ++k) {
    const TickerSeries& t = tickers[k];
    if (count_finite(t.close) < min_bars) continue;

    std::vector<double> rsi_v = rsi(t.close, 14);
    MacdResult macd_r = macd(t.close, 12, 26, 9);
    std::vector<double> dma50 = rolling_mean(t.close, 50);
    std::vector<double> dma200 = rolling_mean(t.close, 200);
    std::vector<double> hi52 = rolling_max(t.close, 252, 150);
    std::vector<double> mfi_v = mfi(t.high, t.low, t.close, t.volume, 14);
    AdxResult adx_r = adx_dmi(t.high, t.low, 14);
    std::vector<double> vol20 = rolling_mean(t.volume, 20);

    std::size_t last = n_dates - 1;
    double px = t.close[last];
    double d50 = dma50[last];
    double d200 = dma200[last];
    double rsi_last = rsi_v[last];
    double mfi_last = mfi_v[last];
    double adx_last = adx_r.adx[last];
    double macd_hist_last = macd_r.histogram[last];
    double dist52 = (px / hi52[last] - 1.0) * 100.0;
    double volr = t.volume[last] / vol20[last];

    bool above200 = px > d200;         // NaN-safe: any comparison with NaN is false
    bool sma_above = d50 > d200;
    bool golden_cross = false;
    if (last >= 1) {
      golden_cross = (d50 > d200) && (dma50[last - 1] <= dma200[last - 1]);
    }
    bool macd_bull = macd_hist_last > 0.0;

    double subs[6];
    subs[0] = clip0_100(rsi_last <= 70.0 ? rsi_last : 70.0 - (rsi_last - 70.0) * 2.0);
    subs[1] = macd_hist_last > 0.0 ? 100.0 : 25.0;  // never NaN: NaN>0 is false -> 25
    subs[2] = (px > d50 && d50 > d200) ? 100.0 : (above200 ? 60.0 : 20.0);  // never NaN
    subs[3] = clip0_100(100.0 + dist52 * 3.0);
    subs[4] = clip0_100((std::isnan(adx_last) ? 20.0 : adx_last) * 2.0);  // nan_to_num first
    subs[5] = clip_upper_100(50.0 * volr);

    double M = mean_skip_nan(subs);
    if (std::isnan(M)) continue;  // res.dropna(subset=["M"])

    double mean_R2_k = kNaN;
    {
      double sum = 0.0;
      int count = 0;
      for (std::size_t i = tail_start; i < n_dates; ++i) {
        if (std::isfinite(returns[k][i])) {
          sum += returns[k][i];
          ++count;
        }
      }
      if (count > 0) mean_R2_k = sum / static_cast<double>(count);
    }
    double beta = kNaN;
    if (!std::isnan(mean_R2_k)) {
      double cov_sum = 0.0;
      int cov_count = 0;
      for (std::size_t i = tail_start; i < n_dates; ++i) {
        if (std::isfinite(returns[k][i]) && std::isfinite(mkt_mean_return[i])) {
          cov_sum += (returns[k][i] - mean_R2_k) * (mkt_mean_return[i] - mean_m2);
          ++cov_count;
        }
      }
      double cov = cov_count > 0 ? cov_sum / static_cast<double>(cov_count) : kNaN;
      beta = cov / beta_denom;
    }

    DvmGlobalMetrics m;
    m.market = market;
    m.ticker = t.ticker;
    m.M = M;
    m.rsi = rsi_last;
    m.mfi = mfi_last;
    m.adx = adx_last;
    m.dist_52w = dist52;
    m.vol_ratio = volr;
    m.beta = beta;
    m.above_200dma = above200;
    m.golden_cross = golden_cross;
    m.sma50_above_200 = sma_above;
    m.macd_bull = macd_bull;
    out.push_back(std::move(m));
  }

  return out;
}

bool screen_momentum_breakout(const DvmGlobalMetrics& r) {
  return r.M >= 70.0 && r.dist_52w >= -10.0 && std::isfinite(r.adx) && r.adx >= 25.0 &&
         r.vol_ratio >= 1.2;
}

bool screen_high_momentum(const DvmGlobalMetrics& r) { return r.M >= 75.0; }

bool screen_golden_crossover(const DvmGlobalMetrics& r) { return r.golden_cross; }

bool screen_uptrend_quality(const DvmGlobalMetrics& r) {
  return r.above_200dma && std::isfinite(r.rsi) && r.rsi >= 50.0 && r.rsi <= 70.0 &&
         std::isfinite(r.adx) && r.adx >= 20.0;
}

bool screen_trendlyne_technical(const DvmGlobalMetrics& r) {
  return std::isfinite(r.rsi) && r.rsi >= 50.0 && r.rsi <= 70.0 && std::isfinite(r.mfi) &&
         r.mfi >= 50.0 && r.macd_bull;
}

bool screen_sma_golden(const DvmGlobalMetrics& r) { return r.sma50_above_200 && r.above_200dma; }

}  // namespace bazaartalks::quant_core
