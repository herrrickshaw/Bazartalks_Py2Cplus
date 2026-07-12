#include "bazaartalks/quant_core/darvas_volume.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "bazaartalks/stats/cross_sectional.hpp"

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
}

DarvasBox darvas_box(const std::vector<double>& high, const std::vector<double>& low,
                     std::size_t lookback, std::size_t confirm, bool exclude_current) {
  std::vector<double> h = high;
  std::vector<double> l = low;
  if (exclude_current && !h.empty() && !l.empty()) {
    h.pop_back();
    l.pop_back();
  }
  if (h.size() < confirm + 2) return DarvasBox{kNaN, kNaN, 0, false};

  std::size_t start = h.size() > lookback ? h.size() - lookback : 0;
  std::vector<double> hw(h.begin() + static_cast<std::ptrdiff_t>(start), h.end());
  std::vector<double> lw(l.begin() + static_cast<std::ptrdiff_t>(start), l.end());

  // np.nanargmax: index of the max, ignoring NaN.
  std::size_t top_i = 0;
  double best = -std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < hw.size(); ++i) {
    if (std::isfinite(hw[i]) && hw[i] > best) {
      best = hw[i];
      top_i = i;
    }
  }
  double top = hw[top_i];

  bool held = (hw.size() - top_i - 1) >= confirm;
  if (held) {
    for (std::size_t i = top_i + 1; i < hw.size(); ++i) {
      if (hw[i] > top + 1e-9) {
        held = false;
        break;
      }
    }
  }

  double bottom;
  if (top_i < lw.size()) {
    double m = std::numeric_limits<double>::infinity();
    bool any = false;
    for (std::size_t i = top_i; i < lw.size(); ++i) {
      if (std::isfinite(lw[i])) {
        m = std::min(m, lw[i]);
        any = true;
      }
    }
    bottom = any ? m : kNaN;
  } else {
    double m = std::numeric_limits<double>::infinity();
    bool any = false;
    for (double v : lw) {
      if (std::isfinite(v)) {
        m = std::min(m, v);
        any = true;
      }
    }
    bottom = any ? m : kNaN;
  }

  return DarvasBox{top, bottom, hw.size() - top_i, held};
}

BoxState box_state(double close_last, const DarvasBox& box, double vol_last, double vol_avg,
                   double vol_surge) {
  if (!std::isfinite(box.top) || !std::isfinite(box.bottom) || box.top <= box.bottom) {
    return BoxState{"no_box", std::nullopt, false};
  }
  std::string state;
  if (close_last > box.top) {
    state = "breakout";
  } else if (close_last < box.bottom) {
    state = "breakdown";
  } else {
    state = "in_box";
  }
  double pos = (close_last - box.bottom) / (box.top - box.bottom);
  pos = std::clamp(pos, 0.0, 1.2);
  pos = std::round(pos * 1000.0) / 1000.0;
  bool vol_confirmed = vol_avg > 0.0 && vol_last >= vol_surge * vol_avg;
  return BoxState{state, pos, vol_confirmed};
}

std::vector<double> accumulation_score(const std::vector<AccumulationFeatures>& features,
                                        const std::vector<double>& eff_ratio) {
  std::size_t n = features.size();
  std::vector<double> obv_t(n), ad_t(n), cmf(n), ud(n), vol_t(n);
  for (std::size_t i = 0; i < n; ++i) {
    obv_t[i] = features[i].obv_trend;
    ad_t[i] = features[i].ad_trend;
    cmf[i] = features[i].cmf;
    vol_t[i] = features[i].vol_trend;
    double r = features[i].ud_vol_ratio;
    if (std::isinf(r) && r > 0) r = kNaN;  // `.replace([np.inf], np.nan)`
    double clipped = std::isnan(r) ? kNaN : std::max(r, 1e-6);
    ud[i] = std::isnan(clipped) ? kNaN : std::log(clipped);
  }
  std::vector<double> eff = eff_ratio.empty() ? std::vector<double>(n, 0.0) : eff_ratio;

  auto z_obv = bazaartalks::stats::zscore(obv_t);
  auto z_ad = bazaartalks::stats::zscore(ad_t);
  auto z_cmf = bazaartalks::stats::zscore(cmf);
  auto z_ud = bazaartalks::stats::zscore(ud);
  auto z_vol = bazaartalks::stats::zscore(vol_t);
  auto z_eff = bazaartalks::stats::zscore(eff);

  std::vector<double> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    double comp = z_obv[i] + z_ad[i] + z_cmf[i] + z_ud[i] + z_vol[i] - z_eff[i];
    out[i] = std::isnan(comp) ? kNaN : std::round(comp * 1000.0) / 1000.0;
  }
  return out;
}

}  // namespace bazaartalks::quant_core
