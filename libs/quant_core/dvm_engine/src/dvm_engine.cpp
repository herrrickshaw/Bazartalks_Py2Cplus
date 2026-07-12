#include "bazaartalks/quant_core/dvm_engine.hpp"

#include "bazaartalks/stats/technical_indicators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// momentum_score() computes each sub-score as a plain Python scalar via
// `min(100, max(0, x))` -- Python's BUILTIN min()/max(), not numpy's. This
// matters when x is NaN: Python's max(a, b) keeps its first argument
// unless the second one compares strictly greater, and a NaN comparison
// is always False -- so `max(0, nan)` returns 0 (first-argument-wins, not
// NaN-propagating), and the subsequent `min(100, 0)` returns 0. The net
// effect is that a NaN sub-score input silently collapses to 0 here, NOT
// to NaN (which numpy's element-wise np.minimum/np.maximum would give,
// and which IS what dvm_global.py's process_market() correctly does on
// its columnar arrays -- see dvm_global.cpp's own, differently-behaved
// clip0_100). Do not merge these two helpers; they replicate genuinely
// different Python behaviors from the two source modules.
double scalar_clip0_100(double x) {
  double maxed = (x > 0.0) ? x : 0.0;
  return (maxed < 100.0) ? maxed : 100.0;
}

// The lone `min(100, x)` case (no enclosing max(0, ...)): a NaN x compares
// False against 100, so Python's min() keeps its first argument -- this
// collapses NaN to 100, not 0. Distinct from scalar_clip0_100 above
// precisely because there's no max(0, ...) wrapping it first.
double scalar_min100(double x) { return (x < 100.0) ? x : 100.0; }
}  // namespace

double momentum_score(const std::vector<double>& close, const std::vector<double>& high,
                      const std::vector<double>& low, const std::vector<double>& volume) {
  using namespace bazaartalks::stats;

  if (close.size() < 220) return kNaN;
  std::size_t last = close.size() - 1;

  std::vector<double> rsi_v = rsi(close, 14);
  MacdResult macd_r = macd(close, 12, 26, 9);
  double dma50 = rolling_mean(close, 50)[last];
  double dma200 = rolling_mean(close, 200)[last];
  double px = close[last];
  // Plain rolling(252).max() -- no min_periods override, unlike
  // dvm_global.py's rolling(252, min_periods=150); with close.size()>=220
  // this window can still be NaN if fewer than 252 bars exist, matching
  // the Python original's own (occasionally NaN) behavior here.
  double hi52 = rolling_max(close, 252)[last];
  AdxResult adx_r = adx_dmi(high, low, 14);
  std::vector<double> vol20 = rolling_mean(volume, 20);

  double rsi_last = rsi_v[last];
  double macd_hist_last = macd_r.histogram[last];
  double adx_last = adx_r.adx[last];
  double vol20_last = vol20[last];
  // `(v.iloc[-1] / v.rolling(20).mean().iloc[-1]) if v.rolling(20).mean().iloc[-1] else 1.0`:
  // Python's bare `if <float>` truthiness -- 0.0 (or NaN, since NaN is
  // truthy in Python but comparing it structurally here doesn't apply;
  // Python's `if nan` IS True) is the only literal falsy float, so this
  // guards specifically against a zero rolling-mean, not a NaN one. A NaN
  // vol20_last is truthy in Python -> takes the division branch -> NaN/NaN
  // semantics, i.e. stays NaN; replicate the same branch selection here.
  double volr;
  if (vol20_last != 0.0) {  // Python: `if vol20_last` is True for NaN too, only False at 0.0
    volr = volume[last] / vol20_last;
  } else {
    volr = 1.0;
  }

  double subs[6];
  subs[0] = scalar_clip0_100(rsi_last <= 70.0 ? rsi_last : 70.0 - (rsi_last - 70.0) * 2.0);
  subs[1] = macd_hist_last > 0.0 ? 100.0 : 25.0;
  subs[2] = (px > dma50 && dma50 > dma200) ? 100.0 : (px > dma200 ? 60.0 : 20.0);
  subs[3] = scalar_clip0_100(100.0 - (hi52 / px - 1.0) * 300.0);
  subs[4] = scalar_clip0_100((std::isnan(adx_last) ? 20.0 : adx_last) * 2.0);
  subs[5] = scalar_min100(50.0 * volr);  // no enclosing max(0, ...), matches dvm_engine.py's sub 6

  // `float(np.mean(subs))` on a plain Python list of 6 scalars -- a
  // straight arithmetic mean, not pandas' skipna mean (there is no pandas
  // object here at all, unlike dvm_global.py's process_market(), which
  // means (axis=1) over a DataFrame). This is moot in practice: every one
  // of the 6 subs above is NaN-proofed by construction (scalar_clip0_100/
  // scalar_min100 collapse a NaN input to a real number, subs[1]/subs[2]
  // are plain ternaries that are never NaN, subs[4] pre-guards adx via
  // nan_to_num-equivalent), so this sum is never actually NaN -- but it's
  // still a plain sum/6, not a skip-NaN average, matching the Python
  // original's own (in this case unexercised) semantics.
  double sum = 0.0;
  for (double v : subs) sum += v;
  return sum / 6.0;
}

double durability_score(std::optional<double> roe, std::optional<double> de,
                        std::optional<bool> fcf_positive, bool revenue_growing,
                        std::optional<double> piotroski_f) {
  double subs[5];
  subs[0] = (roe.has_value() && roe.value() > 20.0)   ? 90.0
            : (roe.has_value() && roe.value() > 15.0) ? 70.0
            : (roe.has_value() && roe.value() > 8.0)  ? 45.0
                                                       : 15.0;
  subs[1] = (de.has_value() && de.value() < 1.0)   ? 85.0
            : (de.has_value() && de.value() < 2.0) ? 50.0
                                                    : 20.0;
  subs[2] = (fcf_positive.has_value() && fcf_positive.value()) ? 80.0 : 20.0;
  subs[3] = revenue_growing ? 75.0 : 35.0;
  subs[4] = piotroski_f.has_value() ? (piotroski_f.value() / 9.0 * 100.0) : 50.0;

  double sum = 0.0;
  for (double v : subs) sum += v;
  return sum / 5.0;
}

char classification_letter(std::optional<double> x) {
  return (x.has_value() && x.value() >= 50.0) ? 'G' : 'B';
}

std::string classification_label(const std::string& code) {
  static const std::unordered_map<std::string, std::string> kLabels = {
      {"GGG", "Strong Performer"},        {"GGB", "Value Under Radar"},
      {"GBG", "Expensive Durable Mover"},  {"GBB", "Expensive Quality"},
      {"BGG", "Cheap Turnaround Mover"},   {"BGB", "Deep Value / Watch"},
      {"BBG", "Momentum Trap"},            {"BBB", "Weak / Avoid"},
  };
  auto it = kLabels.find(code);
  return it != kLabels.end() ? it->second : "-";
}

bool screen_high_dvm(const DvmRow& r) {
  bool d_truthy = r.D.has_value() && r.D.value() != 0.0;
  bool v_truthy = r.V.has_value() && r.V.value() != 0.0;
  bool m_truthy = r.M != 0.0;
  return d_truthy && v_truthy && m_truthy &&
         (r.D.value() + r.V.value() + r.M) / 3.0 >= 65.0;
}

bool screen_durable_momentum(const DvmRow& r) {
  return r.D.value_or(0.0) >= 60.0 && r.M >= 60.0;
}

bool screen_value_under_radar(const DvmRow& r) {
  return r.D.value_or(0.0) >= 55.0 && r.V.value_or(0.0) >= 55.0 && r.M < 50.0;
}

bool screen_momentum_breakout(const DvmRow& r) { return r.M >= 75.0; }

}  // namespace bazaartalks::quant_core
