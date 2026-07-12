#pragma once
// NEW STRATEGY, NOT A PORT: Keltner-Donchian channel-breakout momentum
// with inverse-volatility position sizing and a ratcheting trailing
// stop, from "Refining and Robust Backtesting of a Century of Profitable
// Industry Trends" (arXiv:2412.14361v1) -- a fully deterministic,
// rules-based strategy with no ML/RL training required, chosen from a
// batch of trading-strategy papers specifically because it's the one
// with a complete, closed-form specification (the paper's own other
// candidates were deep-RL agents whose source papers report fragile,
// overfitting-prone results and would require a full neural-net training
// framework this repo doesn't have).
//
// VALIDATION NOTE: unlike every other module in this codebase, there is
// no existing Python original to generate golden fixtures from -- this
// is new C++-native code implementing a published formula. Tests verify
// the formulas against hand-computed/numpy-cross-checked values instead
// of "the actual Python output," and are documented as such per test.
//
// PARAMETER NOTE: the paper being implemented does not state baseline
// numeric values for n (rolling window), k (Keltner multiplier), or
// TargetVol -- it explicitly defers to an earlier paper (Zarattini &
// Antonacci) for those, which was not available to cross-check here.
// Every parameter below is a caller-supplied strategy input, not a
// hardcoded "recommended" constant; the default arguments used in this
// module's own tests are illustrative example values, not calibrated
// trading parameters -- do not treat them as production defaults.

#include <cstddef>
#include <vector>

namespace bazaartalks::quant_core {

struct ChannelBand {
  double upper;
  double lower;
};

// Combined breakout band per bar: upper = min(Donchian upper, Keltner
// upper), lower = max(Donchian lower, Keltner lower) -- the TIGHTER of
// the two channels on each side, matching the paper's "combined band"
// construction (a breakout must clear both channels to enter; a
// stop-out under either channel triggers an exit).
//
//   Donchian(n):  upper = rolling max(close, n) [current bar INCLUDED,
//                 unlike this repo's Darvas-box convention, which
//                 explicitly excludes it -- a deliberate difference,
//                 not an inconsistency, since this is a different
//                 paper's formula]
//   Keltner(n,k): upper = EMA(close,n) + 1.4*k*rolling_mean(|diff(close)|, n)
//
// Returns one band per input bar; NaN upper/lower for the first n-1
// bars (both Donchian and Keltner need `n` bars of history).
std::vector<ChannelBand> combined_channel_band(const std::vector<double>& close, std::size_t n,
                                                double keltner_k = 1.0);

// Ratcheting trailing-stop update: new_stop = max(prev_stop, lower_band_t).
// The stop only ever moves up (tightens), never down -- call this once
// per bar while a position is open, seeding prev_stop with the lower
// band value at entry.
double update_trailing_stop(double prev_stop, double lower_band_t);

// Entry rule: price_t >= upper_band_prev. The caller passes the
// PREVIOUS bar's upper band explicitly (not "the current band lagged
// internally") so the point-in-time contract is visible at the call
// site, matching the paper's own emphasis on `.shift(1)` signal lagging
// to prevent lookahead bias.
bool breakout_entry_signal(double price_t, double upper_band_prev);

// Exit rule: price_t <= trailing_stop_t, where trailing_stop_t is the
// value AFTER update_trailing_stop() has already been applied for this bar.
bool breakout_exit_signal(double price_t, double trailing_stop_t);

// Inverse-volatility position sizing: weight_j = target_vol / vol_j
// (vol_j is each asset's own recent realised volatility, e.g. the
// paper's 14-day rolling vol -- computed by the caller since it's
// already available via bazaartalks::stats::rolling_std on returns).
// If the summed gross exposure exceeds `max_leverage` (paper's own
// example: 200% = 2.0), every weight is rescaled proportionally so the
// total exactly equals max_leverage -- not clipped per-name, matching
// the paper's `omega* = (omega/Exposure) * 200%` formula.
std::vector<double> inverse_vol_weights(const std::vector<double>& asset_vols, double target_vol,
                                        double max_leverage = 2.0);

}  // namespace bazaartalks::quant_core
