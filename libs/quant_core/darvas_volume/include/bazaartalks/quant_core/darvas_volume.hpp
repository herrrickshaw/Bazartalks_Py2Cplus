#pragma once
// Port of darvas_volume.py's box-formation/scan logic. The underlying
// volume-acquisition indicators (obv, chaikin_ad, chaikin_money_flow,
// up_down_volume_ratio) and trend_corr were already ported in Phase 1
// (libs/stats/technical_indicators.hpp, libs/stats/cross_sectional.hpp)
// -- this module is the box math and the composite score that sit on top
// of them. Report 1's own assessment: "low -- all indicators are simple
// array scans, very portable."

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

struct DarvasBox {
  double top;
  double bottom;
  std::size_t len = 0;
  bool held = false;
};

// Forms the current Darvas box from highs/lows. Design rule (preserved,
// not "fixed"): the current bar is EXCLUDED from box formation by
// default, since including it would make a same-bar breakout/breakdown
// detectable by construction impossible. box.top is NaN (via a
// default-constructed DarvasBox with top/bottom left uninitialized to
// NaN by the caller-visible convention below) if there's insufficient
// history.
DarvasBox darvas_box(const std::vector<double>& high, const std::vector<double>& low,
                     std::size_t lookback = 40, std::size_t confirm = 3,
                     bool exclude_current = true);

struct BoxState {
  std::string state;  // "no_box" | "breakout" | "breakdown" | "in_box"
  std::optional<double> position;
  bool vol_confirmed = false;
};

// Classifies the current bar against a box: breakout/breakdown/in_box,
// its position in [0, 1.2] (clipped, allowing a little overshoot above
// the top before it's called a full breakout position), and whether a
// breakout/breakdown is volume-confirmed (>= vol_surge x the box's
// average volume).
BoxState box_state(double close_last, const DarvasBox& box, double vol_last, double vol_avg,
                   double vol_surge = 1.5);

struct AccumulationFeatures {
  double obv_trend;
  double ad_trend;
  double cmf;
  double ud_vol_ratio;
  double vol_trend;
};

// Cross-sectional composite over a batch: OBV/AD/volume up-trends + CMF
// + up/down-volume ratio (log-transformed to be symmetric around 0),
// minus efficiency ratio (a pinned/low-efficiency price = mark-time
// accumulation). Higher = more volume being quietly acquired.
// `eff_ratio` defaults to an implicit all-zero series when omitted,
// matching Python's `feat.get("eff_ratio", pd.Series(0, ...))` --
// z-scoring an all-zero (degenerate) series yields 0 for every row, so
// omitting it is equivalent to not penalising efficiency at all.
std::vector<double> accumulation_score(const std::vector<AccumulationFeatures>& features,
                                        const std::vector<double>& eff_ratio = {});

}  // namespace bazaartalks::quant_core
