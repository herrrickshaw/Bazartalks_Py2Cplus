#pragma once
// Port of unlisted_valuation.py's pure comps-valuation core: apply a
// listed-peer trading multiple (P/E, P/B) to a private firm's financial
// metric, with a median + IQR band from the peer set. Report 1's own
// assessment: "trivial... implied_value/peer_multiple_band/value_range are
// simple scalar/array-percentile functions."

#include <cstddef>
#include <optional>
#include <vector>

namespace bazaartalks::quant_core {

// value = metric * multiple (e.g. earnings * P/E, book * P/B).
double implied_value(double metric_value, double multiple);

struct MultipleBand {
  std::size_t n = 0;
  std::optional<double> median;
  std::optional<double> q1;
  std::optional<double> q3;
};

// Robust central multiple + IQR band from a peer set: drops non-finite
// and non-positive values first (meaningless for a trading multiple),
// matching Python's `m[np.isfinite(m) & (m > 0)]` filter. Returns an
// all-nullopt band (n=0) if nothing survives the filter.
MultipleBand peer_multiple_band(const std::vector<double>& multiples);

struct ValueRange {
  std::optional<double> low;
  std::optional<double> mid;
  std::optional<double> high;
  std::size_t n_peers = 0;
};

// Applies a multiple band to a financial metric -> implied value range.
// Returns an all-nullopt range if the band has no median (no peer
// coverage), matching Python's `if not band or band.get("median") is
// None`.
ValueRange value_range(double metric_value, const MultipleBand& band);

}  // namespace bazaartalks::quant_core
