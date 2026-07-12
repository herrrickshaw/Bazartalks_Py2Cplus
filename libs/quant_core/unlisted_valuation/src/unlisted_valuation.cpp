#include "bazaartalks/quant_core/unlisted_valuation.hpp"

#include <cmath>

#include "bazaartalks/quant_core/risk.hpp"  // reuse quantile_linear (same numpy formula
                                            // np.median()/np.quantile() both need)

namespace bazaartalks::quant_core {

double implied_value(double metric_value, double multiple) { return metric_value * multiple; }

MultipleBand peer_multiple_band(const std::vector<double>& multiples) {
  std::vector<double> m;
  m.reserve(multiples.size());
  for (double x : multiples) {
    if (std::isfinite(x) && x > 0.0) m.push_back(x);
  }
  if (m.empty()) return MultipleBand{};

  MultipleBand band;
  band.n = m.size();
  band.median = quantile_linear(m, 0.5);  // np.median == np.quantile(..., 0.5)
  band.q1 = quantile_linear(m, 0.25);
  band.q3 = quantile_linear(m, 0.75);
  return band;
}

ValueRange value_range(double metric_value, const MultipleBand& band) {
  if (!band.median) return ValueRange{};
  return ValueRange{
      implied_value(metric_value, *band.q1),
      implied_value(metric_value, *band.median),
      implied_value(metric_value, *band.q3),
      band.n,
  };
}

}  // namespace bazaartalks::quant_core
