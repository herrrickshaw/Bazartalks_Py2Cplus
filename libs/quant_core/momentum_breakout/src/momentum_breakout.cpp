#include "bazaartalks/quant_core/momentum_breakout.hpp"

#include "bazaartalks/stats/technical_indicators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
}

std::vector<ChannelBand> combined_channel_band(const std::vector<double>& close, std::size_t n,
                                                double keltner_k) {
  using bazaartalks::stats::ewm_mean;
  using bazaartalks::stats::rolling_max;
  using bazaartalks::stats::rolling_mean;
  using bazaartalks::stats::rolling_min;

  std::size_t size = close.size();
  std::vector<double> abs_diff(size, kNaN);
  for (std::size_t i = 1; i < size; ++i) abs_diff[i] = std::abs(close[i] - close[i - 1]);

  auto ema = ewm_mean(close, static_cast<int>(n));
  auto mean_abs_move = rolling_mean(abs_diff, n);
  auto donchian_up = rolling_max(close, n);
  auto donchian_dn = rolling_min(close, n);

  // pandas' `pd.concat([donchian, keltner], axis=1).min(axis=1)` (and
  // `.max(axis=1)`) SKIP NaN by default -- if only one side has enough
  // history yet, the result is that side's value, not NaN. Donchian
  // needs only `n` raw closes; Keltner additionally needs `n` valid
  // |diff(close)| values, which lag by one extra bar (diff's first
  // entry is always NaN) -- so for one bar, Donchian is valid while
  // Keltner isn't, and the combined band must fall back to Donchian
  // alone there rather than propagating NaN.
  std::vector<ChannelBand> out(size);
  for (std::size_t i = 0; i < size; ++i) {
    bool keltner_ready = std::isfinite(mean_abs_move[i]);
    bool donchian_ready = std::isfinite(donchian_up[i]);
    if (!keltner_ready && !donchian_ready) {
      out[i] = ChannelBand{kNaN, kNaN};
      continue;
    }
    double upper, lower;
    if (keltner_ready && donchian_ready) {
      double keltner_up = ema[i] + 1.4 * keltner_k * mean_abs_move[i];
      double keltner_dn = ema[i] - 1.4 * keltner_k * mean_abs_move[i];
      upper = std::min(donchian_up[i], keltner_up);
      lower = std::max(donchian_dn[i], keltner_dn);
    } else if (donchian_ready) {
      upper = donchian_up[i];
      lower = donchian_dn[i];
    } else {
      upper = ema[i] + 1.4 * keltner_k * mean_abs_move[i];
      lower = ema[i] - 1.4 * keltner_k * mean_abs_move[i];
    }
    out[i] = ChannelBand{upper, lower};
  }
  return out;
}

double update_trailing_stop(double prev_stop, double lower_band_t) {
  return std::max(prev_stop, lower_band_t);
}

bool breakout_entry_signal(double price_t, double upper_band_prev) {
  return price_t >= upper_band_prev;
}

bool breakout_exit_signal(double price_t, double trailing_stop_t) {
  return price_t <= trailing_stop_t;
}

std::vector<double> inverse_vol_weights(const std::vector<double>& asset_vols, double target_vol,
                                        double max_leverage) {
  std::vector<double> w(asset_vols.size());
  double exposure = 0.0;
  for (std::size_t i = 0; i < asset_vols.size(); ++i) {
    w[i] = asset_vols[i] > 0.0 ? target_vol / asset_vols[i] : 0.0;
    exposure += w[i];
  }
  if (exposure > max_leverage && exposure > 0.0) {
    double scale = max_leverage / exposure;
    for (double& x : w) x *= scale;
  }
  return w;
}

}  // namespace bazaartalks::quant_core
