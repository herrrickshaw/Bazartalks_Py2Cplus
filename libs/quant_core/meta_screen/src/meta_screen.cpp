#include "bazaartalks/quant_core/meta_screen.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace bazaartalks::quant_core {

double fuse(const ScreenComponents& c, const ScreenWeights& w, int num_gates_satisfied,
            double gate_bonus) {
  double num = 0.0, den = 0.0;
  // (score, weight) pairs -- skip anything absent, matching Python's
  // `if score is None or isnan(score): continue`.
  const std::array<std::pair<const std::optional<double>&, double>, 7> components = {{
      {c.durability, w.durability},
      {c.valuation, w.valuation},
      {c.momentum, w.momentum},
      {c.ml_signal, w.ml_signal},
      {c.quality, w.quality},
      {c.pead, w.pead},
      {c.liquidity, w.liquidity},
  }};
  for (const auto& [score, weight] : components) {
    if (!score.has_value() || weight <= 0.0) continue;
    num += weight * *score;
    den += weight;
  }
  double base = den > 0.0 ? num / den : 0.0;
  double bonus = gate_bonus * static_cast<double>(num_gates_satisfied);
  return std::min(100.0, std::max(0.0, base + bonus));
}

}  // namespace bazaartalks::quant_core
