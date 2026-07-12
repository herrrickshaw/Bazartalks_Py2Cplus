#include "bazaartalks/quant_core/net_of_cost.hpp"

#include <algorithm>
#include <limits>

namespace bazaartalks::quant_core {

const std::unordered_map<std::string, double> kRoundTripByMarket = {
    {"US", 0.0010}, {"IN", 0.0030}, {"India", 0.0030}, {"JP", 0.0020}, {"Japan", 0.0020},
    {"KR", 0.0025}, {"Korea", 0.0025}, {"EU", 0.0040}, {"Europe", 0.0040}, {"UK", 0.0025},
    {"BR", 0.0035}, {"CA", 0.0015}, {"DE", 0.0030}, {"SG", 0.0020}, {"ZA", 0.0030},
};

double round_trip_cost_for_market(const std::string& market) {
  auto it = kRoundTripByMarket.find(market);
  return it != kRoundTripByMarket.end() ? it->second : kDefaultRoundTrip;
}

double amihud_impact(double amihud_illiq, double trade_value) {
  if (amihud_illiq <= 0.0 || trade_value <= 0.0) return 0.0;
  return std::max(0.0, std::min(0.5, amihud_illiq * trade_value));
}

double round_trip_cost(double commission, double half_spread, double amihud_illiq,
                        double trade_value) {
  double impact = amihud_impact(amihud_illiq, trade_value);
  double per_side = std::max(0.0, commission) + std::max(0.0, half_spread) + impact;
  return 2.0 * per_side;
}

double net_spread(double gross_spread, double round_trip, double turnover) {
  return gross_spread - std::max(0.0, turnover) * std::max(0.0, round_trip);
}

double break_even_cost(double gross_spread, double turnover) {
  if (turnover <= 0.0) return std::numeric_limits<double>::infinity();
  return gross_spread / turnover;
}

bool survives_costs(double gross_spread, const std::string& market, double turnover,
                     const double* round_trip_override) {
  double rt = round_trip_override ? *round_trip_override : round_trip_cost_for_market(market);
  return break_even_cost(gross_spread, turnover) > rt;
}

}  // namespace bazaartalks::quant_core
