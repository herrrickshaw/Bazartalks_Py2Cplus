#pragma once
// Rolling-window technical-indicator kernels shared by dvm_engine.py/
// dvm_global.py (RSI/MACD/ADX-DMI, Phase 5) and darvas_volume.py
// (OBV/Chaikin A-D/CMF/up-down-volume-ratio, Phase 3). Built once here per
// the migration plan so both phases reuse the same, golden-tested kernels
// instead of duplicating the logic.
//
// NaN-propagation semantics matter as much as the arithmetic: pandas'
// rolling(window).mean()/.max() with the (default) min_periods=window
// convention returns NaN unless the full window is present, and both
// pandas .diff() (first entry NaN) and numpy .diff(x, prepend=x[0]) (first
// entry 0.0) are used in different places in the Python originals -- mixing
// them up silently shifts every downstream indicator by one bar. Each
// function below documents which convention it follows and why.

#include <cstddef>
#include <optional>
#include <vector>

namespace bazaartalks::stats {

// pandas Series.diff(): out[0] = NaN, out[i] = x[i] - x[i-1].
// Used ahead of RSI/ADX-DMI, matching dvm_engine.py's `d = c.diff()` /
// `up = df["High"].diff()` / `dn = -df["Low"].diff()`.
std::vector<double> diff_pandas(const std::vector<double>& x);

// numpy.diff(x, prepend=x[0]): out[0] = 0.0 (not NaN), out[i] = x[i]-x[i-1].
// Used ahead of OBV/up_down_volume_ratio, matching darvas_volume.py's
// `np.diff(c, prepend=c[0])`.
std::vector<double> diff_numpy_prepend_first(const std::vector<double>& x);

// pandas Series.rolling(window).mean() with the default min_periods=window:
// out[i] = NaN for i < window-1; otherwise the plain mean of x[i-window+1..i]
// -- NaN propagates if ANY value in that span is NaN (all-or-nothing, not a
// partial/skip-NaN average).
std::vector<double> rolling_mean(const std::vector<double>& x, std::size_t window);

// Same min_periods=window convention as rolling_mean, but the max over the
// window (used by momentum_score()'s 52-week-high proximity check).
std::vector<double> rolling_max(const std::vector<double>& x, std::size_t window);

// pandas Series.rolling(window).std() -- SAMPLE std (ddof=1, pandas'
// default), same min_periods=window all-or-nothing NaN convention as
// rolling_mean/rolling_max.
std::vector<double> rolling_std(const std::vector<double>& x, std::size_t window);

// pandas Series.ewm(span=N).mean() with adjust=True (pandas' default):
// a weighted average of the full history so far, NOT a simple recursive
// EMA -- see the .cpp for the exact recursive num/den formulation. Assumes
// `x` has no NaN entries (true for the clean OHLC series this is applied
// to in every Python caller, per stock_utils.clean_ohlcv's upstream
// guarantee).
std::vector<double> ewm_mean(const std::vector<double>& x, int span);

// pandas Series.ewm(span=N, adjust=False).mean(): a plain recursive EMA,
// y[0]=x[0], y[t] = alpha*x[t] + (1-alpha)*y[t-1]. Distinct from
// ewm_mean() above (adjust=True) -- ml_signal_engine.py's MACD uses
// adjust=False explicitly, while dvm_engine.py's MACD (Phase 5) uses the
// adjust=True default; do not conflate the two.
std::vector<double> ewm_mean_no_adjust(const std::vector<double>& x, int span);

// Port of momentum_score()'s inline RSI: rolling(14) average gain/loss,
// with the average-loss-of-exactly-zero -> NaN guard replicated verbatim
// (`.replace(0, np.nan)`) -- a pure uptrend with zero losses in the window
// yields RSI = NaN in the Python original, not 100; do not "fix" this.
std::vector<double> rsi(const std::vector<double>& close, std::size_t window = 14);

struct MacdResult {
  std::vector<double> macd_line;
  std::vector<double> signal_line;
  std::vector<double> histogram;
};

// Port of momentum_score()'s MACD: ewm(fast) - ewm(slow), signal = ewm(macd).
MacdResult macd(const std::vector<double>& close, int fast = 12, int slow = 26, int signal = 9);

struct AdxResult {
  std::vector<double> plus_di;
  std::vector<double> minus_di;
  std::vector<double> adx;
};

// Port of momentum_score()'s ADX/DMI(14). Note: this uses a SIMPLIFIED
// "true range" of plain (High-Low), not the textbook true range that also
// considers the previous close -- replicated as-is from dvm_engine.py, not
// "corrected", since the Python original's downstream scores are tuned
// against this exact (simplified) formula.
AdxResult adx_dmi(const std::vector<double>& high, const std::vector<double>& low,
                  std::size_t window = 14);

// Port of darvas_volume.py's obv(): cumulative signed volume using the
// numpy diff-with-prepend convention (first bar contributes 0, not NaN).
std::vector<double> obv(const std::vector<double>& close, const std::vector<double>& volume);

// Port of darvas_volume.py's chaikin_ad(): cumulative money-flow volume.
// Bars where High == Low (zero range) contribute 0 to the cumulative sum
// (`np.nan_to_num` on the money-flow multiplier), not NaN or a skipped bar.
std::vector<double> chaikin_ad(const std::vector<double>& high, const std::vector<double>& low,
                                const std::vector<double>& close,
                                const std::vector<double>& volume);

// Port of darvas_volume.py's chaikin_money_flow(): sum(money-flow-volume) /
// sum(volume) over the trailing `period` bars (or the whole series if
// period is empty). Returns NaN if total volume is 0.
double chaikin_money_flow(const std::vector<double>& high, const std::vector<double>& low,
                           const std::vector<double>& close, const std::vector<double>& volume,
                           std::optional<std::size_t> period = std::nullopt);

// Port of darvas_volume.py's up_down_volume_ratio(). Edge cases replicated
// exactly: down-volume of 0 with positive up-volume -> +infinity; both 0
// -> NaN.
double up_down_volume_ratio(const std::vector<double>& close, const std::vector<double>& volume);

}  // namespace bazaartalks::stats
