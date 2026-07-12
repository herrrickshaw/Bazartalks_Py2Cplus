#pragma once
// Port of net_of_cost.py -- pure scalar cost-model formulas, no numpy/pandas
// dependency at all in the original. Report 1's own assessment: "ideal
// first C++ port... every function is a stateless scalar formula."

#include <string>
#include <unordered_map>

namespace bazaartalks::quant_core {

// Realistic retail round-trip cost per market, as a fraction of notional.
// Matches net_of_cost.py's ROUND_TRIP dict verbatim (including both the
// short ISO-ish codes and the long market names as separate keys, exactly
// as the Python dict has both "IN" and "India" mapping to the same value).
extern const std::unordered_map<std::string, double> kRoundTripByMarket;
constexpr double kDefaultRoundTrip = 0.0030;

double round_trip_cost_for_market(const std::string& market);

// Fractional price impact of a trade from the Amihud illiquidity measure,
// clipped to [0, 0.5]. Returns 0 if either input is non-positive.
double amihud_impact(double amihud_illiq, double trade_value);

double round_trip_cost(double commission = 0.0005, double half_spread = 0.0005,
                        double amihud_illiq = 0.0, double trade_value = 0.0);

double net_spread(double gross_spread, double round_trip, double turnover = 1.0);

// Round-trip cost (fraction) that exactly zeroes the net spread. Returns
// +infinity if turnover <= 0, matching Python's `float("inf")`.
double break_even_cost(double gross_spread, double turnover = 1.0);

// True iff the signal's break-even cost exceeds the market's realistic
// round-trip cost. `round_trip` overrides the market lookup when provided
// (matches the Python original's `round_trip: float | None = None`).
bool survives_costs(double gross_spread, const std::string& market = "US", double turnover = 1.0,
                     const double* round_trip_override = nullptr);

}  // namespace bazaartalks::quant_core
