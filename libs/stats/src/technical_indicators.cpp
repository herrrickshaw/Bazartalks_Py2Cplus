#include "bazaartalks/stats/technical_indicators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bazaartalks::stats {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
}  // namespace

std::vector<double> diff_pandas(const std::vector<double>& x) {
  std::vector<double> out(x.size(), kNaN);
  for (std::size_t i = 1; i < x.size(); ++i) out[i] = x[i] - x[i - 1];
  return out;
}

std::vector<double> diff_numpy_prepend_first(const std::vector<double>& x) {
  std::vector<double> out(x.size(), 0.0);
  for (std::size_t i = 1; i < x.size(); ++i) out[i] = x[i] - x[i - 1];
  return out;
}

std::vector<double> rolling_mean(const std::vector<double>& x, std::size_t window) {
  std::vector<double> out(x.size(), kNaN);
  if (window == 0 || window > x.size()) return out;
  for (std::size_t i = window - 1; i < x.size(); ++i) {
    double sum = 0.0;
    for (std::size_t j = i + 1 - window; j <= i; ++j) sum += x[j];  // NaN propagates via IEEE arith
    out[i] = sum / static_cast<double>(window);
  }
  return out;
}

std::vector<double> rolling_max(const std::vector<double>& x, std::size_t window,
                                std::size_t min_periods) {
  std::size_t n = x.size();
  std::vector<double> out(n, kNaN);
  if (window == 0) return out;
  std::size_t required = min_periods == 0 ? window : min_periods;

  for (std::size_t i = 0; i < n; ++i) {
    std::size_t start = i + 1 >= window ? i + 1 - window : 0;
    double m = kNaN;
    std::size_t count = 0;
    for (std::size_t j = start; j <= i; ++j) {
      if (std::isfinite(x[j])) {
        if (count == 0 || x[j] > m) m = x[j];
        ++count;
      }
    }
    out[i] = count >= required ? m : kNaN;
  }
  return out;
}

std::vector<double> rolling_min(const std::vector<double>& x, std::size_t window,
                                std::size_t min_periods) {
  std::size_t n = x.size();
  std::vector<double> out(n, kNaN);
  if (window == 0) return out;
  std::size_t required = min_periods == 0 ? window : min_periods;

  for (std::size_t i = 0; i < n; ++i) {
    std::size_t start = i + 1 >= window ? i + 1 - window : 0;
    double m = kNaN;
    std::size_t count = 0;
    for (std::size_t j = start; j <= i; ++j) {
      if (std::isfinite(x[j])) {
        if (count == 0 || x[j] < m) m = x[j];
        ++count;
      }
    }
    out[i] = count >= required ? m : kNaN;
  }
  return out;
}

std::vector<double> rolling_std(const std::vector<double>& x, std::size_t window) {
  std::vector<double> out(x.size(), kNaN);
  if (window < 2 || window > x.size()) return out;
  for (std::size_t i = window - 1; i < x.size(); ++i) {
    double sum = 0.0;
    for (std::size_t j = i + 1 - window; j <= i; ++j) sum += x[j];
    double mean = sum / static_cast<double>(window);
    double sq = 0.0;
    for (std::size_t j = i + 1 - window; j <= i; ++j) sq += (x[j] - mean) * (x[j] - mean);
    out[i] = std::sqrt(sq / static_cast<double>(window - 1));  // ddof=1, pandas' default
  }
  return out;
}

std::vector<double> ewm_mean_no_adjust(const std::vector<double>& x, int span) {
  std::vector<double> out(x.size(), kNaN);
  if (x.empty()) return out;
  double alpha = 2.0 / (static_cast<double>(span) + 1.0);
  out[0] = x[0];
  for (std::size_t i = 1; i < x.size(); ++i) {
    out[i] = alpha * x[i] + (1.0 - alpha) * out[i - 1];
  }
  return out;
}

std::vector<double> ewm_mean(const std::vector<double>& x, int span) {
  std::vector<double> out(x.size(), kNaN);
  if (x.empty()) return out;
  double alpha = 2.0 / (static_cast<double>(span) + 1.0);
  double beta = 1.0 - alpha;
  double num = x[0];
  double den = 1.0;
  out[0] = num / den;
  for (std::size_t i = 1; i < x.size(); ++i) {
    num = x[i] + beta * num;
    den = 1.0 + beta * den;
    out[i] = num / den;
  }
  return out;
}

std::vector<double> rsi(const std::vector<double>& close, std::size_t window) {
  std::vector<double> d = diff_pandas(close);
  std::vector<double> gain(d.size()), loss(d.size());
  for (std::size_t i = 0; i < d.size(); ++i) {
    gain[i] = std::isnan(d[i]) ? kNaN : std::max(d[i], 0.0);
    loss[i] = std::isnan(d[i]) ? kNaN : std::max(-d[i], 0.0);
  }
  std::vector<double> avg_gain = rolling_mean(gain, window);
  std::vector<double> avg_loss = rolling_mean(loss, window);

  std::vector<double> out(close.size(), kNaN);
  for (std::size_t i = 0; i < close.size(); ++i) {
    double al = avg_loss[i] == 0.0 ? kNaN : avg_loss[i];  // `.replace(0, np.nan)`
    out[i] = 100.0 - 100.0 / (1.0 + avg_gain[i] / al);
  }
  return out;
}

MacdResult macd(const std::vector<double>& close, int fast, int slow, int signal) {
  std::vector<double> ema_fast = ewm_mean(close, fast);
  std::vector<double> ema_slow = ewm_mean(close, slow);
  std::vector<double> macd_line(close.size());
  for (std::size_t i = 0; i < close.size(); ++i) macd_line[i] = ema_fast[i] - ema_slow[i];
  std::vector<double> signal_line = ewm_mean(macd_line, signal);
  std::vector<double> hist(close.size());
  for (std::size_t i = 0; i < close.size(); ++i) hist[i] = macd_line[i] - signal_line[i];
  return MacdResult{std::move(macd_line), std::move(signal_line), std::move(hist)};
}

AdxResult adx_dmi(const std::vector<double>& high, const std::vector<double>& low,
                   std::size_t window) {
  std::size_t n = high.size();
  std::vector<double> up = diff_pandas(high);
  std::vector<double> dn(n);
  {
    std::vector<double> low_diff = diff_pandas(low);
    for (std::size_t i = 0; i < n; ++i) dn[i] = -low_diff[i];
  }

  std::vector<double> plus_dm(n), minus_dm(n), range(n);
  for (std::size_t i = 0; i < n; ++i) {
    // NaN comparisons are false in IEEE 754, so index 0 (up[0]/dn[0] == NaN)
    // naturally yields 0.0 here, matching numpy's np.where((up>dn)&(up>0),...).
    plus_dm[i] = (up[i] > dn[i] && up[i] > 0.0) ? up[i] : 0.0;
    minus_dm[i] = (dn[i] > up[i] && dn[i] > 0.0) ? dn[i] : 0.0;
    range[i] = high[i] - low[i];
  }

  std::vector<double> tr = rolling_mean(range, window);
  std::vector<double> plus_dm_avg = rolling_mean(plus_dm, window);
  std::vector<double> minus_dm_avg = rolling_mean(minus_dm, window);

  std::vector<double> pdi(n), mdi(n), dx(n);
  for (std::size_t i = 0; i < n; ++i) {
    pdi[i] = 100.0 * plus_dm_avg[i] / tr[i];
    mdi[i] = 100.0 * minus_dm_avg[i] / tr[i];
    double denom = pdi[i] + mdi[i];
    double denom_adj = denom == 0.0 ? kNaN : denom;  // `.replace(0, np.nan)`
    dx[i] = 100.0 * std::abs(pdi[i] - mdi[i]) / denom_adj;
  }
  std::vector<double> adx = rolling_mean(dx, window);

  return AdxResult{std::move(pdi), std::move(mdi), std::move(adx)};
}

std::vector<double> obv(const std::vector<double>& close, const std::vector<double>& volume) {
  std::vector<double> d = diff_numpy_prepend_first(close);
  std::vector<double> out(close.size());
  double running = 0.0;
  for (std::size_t i = 0; i < close.size(); ++i) {
    double sign = d[i] > 0.0 ? 1.0 : (d[i] < 0.0 ? -1.0 : 0.0);
    running += sign * volume[i];
    out[i] = running;
  }
  return out;
}

std::vector<double> chaikin_ad(const std::vector<double>& high, const std::vector<double>& low,
                                const std::vector<double>& close,
                                const std::vector<double>& volume) {
  std::vector<double> out(high.size());
  double running = 0.0;
  for (std::size_t i = 0; i < high.size(); ++i) {
    double range = high[i] - low[i];
    double mfm = range > 0.0 ? (((close[i] - low[i]) - (high[i] - close[i])) / range) : kNaN;
    if (std::isnan(mfm)) mfm = 0.0;  // np.nan_to_num
    running += mfm * volume[i];
    out[i] = running;
  }
  return out;
}

double chaikin_money_flow(const std::vector<double>& high, const std::vector<double>& low,
                           const std::vector<double>& close, const std::vector<double>& volume,
                           std::optional<std::size_t> period) {
  std::size_t n = high.size();
  std::size_t start = 0;
  if (period && *period < n) start = n - *period;

  double num = 0.0, vol_total = 0.0;
  for (std::size_t i = start; i < n; ++i) {
    double range = high[i] - low[i];
    double mfm = range > 0.0 ? (((close[i] - low[i]) - (high[i] - close[i])) / range) : kNaN;
    if (std::isnan(mfm)) mfm = 0.0;
    num += mfm * volume[i];
    vol_total += volume[i];
  }
  return vol_total > 0.0 ? num / vol_total : kNaN;
}

double up_down_volume_ratio(const std::vector<double>& close, const std::vector<double>& volume) {
  std::vector<double> d = diff_numpy_prepend_first(close);
  double up = 0.0, down = 0.0;
  for (std::size_t i = 0; i < close.size(); ++i) {
    if (d[i] > 0.0) up += volume[i];
    if (d[i] < 0.0) down += volume[i];
  }
  if (down > 0.0) return up / down;
  return up > 0.0 ? kInf : kNaN;
}

std::vector<double> mfi(const std::vector<double>& high, const std::vector<double>& low,
                        const std::vector<double>& close, const std::vector<double>& volume,
                        std::size_t window) {
  std::size_t n = close.size();
  std::vector<double> tp(n), rmf(n);
  for (std::size_t i = 0; i < n; ++i) {
    tp[i] = (high[i] + low[i] + close[i]) / 3.0;
    rmf[i] = tp[i] * volume[i];
  }
  std::vector<double> tpd = diff_pandas(tp);  // tpd[0] = NaN

  std::vector<double> pos(n), neg(n);
  for (std::size_t i = 0; i < n; ++i) {
    // dvm_global.py's process_market()/_tech() use `rmf.where(tpd > 0, 0.0)`
    // (and the `< 0` mirror for neg) -- pandas' `.where(cond, other)` keeps
    // the original value where cond is True and substitutes `other`
    // otherwise. A NaN comparison (i=0, where tpd is undefined) is False,
    // so both pos[0] and neg[0] fall through to the 0.0 substitute -- same
    // "neither up nor down" treatment as any other non-matching day.
    pos[i] = tpd[i] > 0.0 ? rmf[i] : 0.0;
    neg[i] = tpd[i] < 0.0 ? rmf[i] : 0.0;
  }
  // rolling(window).sum() == rolling_mean(...) * window, since rolling_mean's
  // all-or-nothing NaN convention matches pandas' default min_periods=window
  // and pos/neg themselves never contain NaN (real 0.0 fallback above).
  std::vector<double> pos_sum = rolling_mean(pos, window);
  std::vector<double> neg_sum = rolling_mean(neg, window);

  std::vector<double> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    double ps = pos_sum[i] * static_cast<double>(window);
    double ns = neg_sum[i] * static_cast<double>(window);
    double ns_adj = ns == 0.0 ? kNaN : ns;  // `.replace(0, np.nan)`
    out[i] = 100.0 - 100.0 / (1.0 + ps / ns_adj);
  }
  return out;
}

}  // namespace bazaartalks::stats
