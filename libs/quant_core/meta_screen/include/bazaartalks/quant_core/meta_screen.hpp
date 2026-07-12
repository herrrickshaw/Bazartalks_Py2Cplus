#pragma once
// Port of meta_screen.py's fuse() -- the pure signal-fusion core: a
// weighted mean over whichever components are present (missing
// components drop out and the remaining weights renormalise), plus a
// flat bonus per satisfied hard gate (e.g. "Triple-Hit"), clamped to
// [0, 100]. Report 1's own assessment: "trivial-low... a simple weighted
// average with None/NaN-skipping and renormalization -- directly
// portable."
//
// Design note: Python's `gates` parameter is a {name: bool} dict, but in
// every actual caller (rank()) only one gate ("triple_hit") is ever used;
// this port generalizes it as a satisfied-gate COUNT instead of a named
// map, which preserves fuse()'s real contract (bonus = gate_bonus *
// number of true gates) without a string-keyed map for a single-gate
// use case.

#include <optional>

namespace bazaartalks::quant_core {

struct ScreenComponents {
  std::optional<double> durability;
  std::optional<double> valuation;
  std::optional<double> momentum;
  std::optional<double> ml_signal;
  std::optional<double> quality;
  std::optional<double> pead;
  std::optional<double> liquidity;
};

// Matches meta_screen.py's DEFAULT_WEIGHTS verbatim.
struct ScreenWeights {
  double durability = 0.20;
  double valuation = 0.12;
  double momentum = 0.16;
  double ml_signal = 0.16;
  double quality = 0.16;
  double pead = 0.12;
  double liquidity = 0.08;
};

constexpr double kDefaultGateBonus = 10.0;

double fuse(const ScreenComponents& components, const ScreenWeights& weights = {},
            int num_gates_satisfied = 0, double gate_bonus = kDefaultGateBonus);

}  // namespace bazaartalks::quant_core
