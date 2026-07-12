#include "bazaartalks/quant_core/fx.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

namespace bazaartalks::quant_core {

const std::unordered_map<std::string, std::string> kMarketCurrency = {
    {"AU", "AUD"}, {"BR", "BRL"}, {"CA", "CAD"}, {"CH", "CHF"}, {"CN", "CNY"}, {"DE", "EUR"},
    {"DK", "DKK"}, {"EU", "EUR"}, {"FI", "EUR"}, {"HK", "HKD"}, {"JP", "JPY"}, {"KR", "KRW"},
    {"SA", "SAR"}, {"SE", "SEK"}, {"SG", "SGD"}, {"TW", "TWD"}, {"UK", "GBP"}, {"US", "USD"},
    {"ZA", "ZAR"},
};

const std::unordered_map<std::string, double> kSnapshotUsdPer = {
    {"USD", 1.0},    {"EUR", 1.08},   {"GBP", 1.27},   {"JPY", 0.0064}, {"KRW", 0.00073},
    {"AUD", 0.66},   {"BRL", 0.18},   {"CAD", 0.73},   {"CHF", 1.11},   {"CNY", 0.138},
    {"DKK", 0.145},  {"HKD", 0.128},  {"SAR", 0.267},  {"SEK", 0.094},  {"SGD", 0.74},
    {"TWD", 0.031},  {"ZAR", 0.054},
};

std::string market_currency(const std::string& market) {
  std::string upper = market;
  std::transform(upper.begin(), upper.end(), upper.begin(),
                  [](unsigned char ch) { return std::toupper(ch); });
  auto it = kMarketCurrency.find(upper);
  return it != kMarketCurrency.end() ? it->second : "USD";
}

double convert_level(double amount, double rate_base_per_local) {
  return amount * rate_base_per_local;
}

double combine_return(double r_local, double r_fx) {
  return (1.0 + r_local) * (1.0 + r_fx) - 1.0;
}

std::unordered_map<std::string, double> load_rates_snapshot(const std::string& base) {
  if (base == "USD") return kSnapshotUsdPer;

  auto it = kSnapshotUsdPer.find(base);
  if (it == kSnapshotUsdPer.end()) return kSnapshotUsdPer;  // unknown base -> Python's fallback

  double b = it->second;
  std::unordered_map<std::string, double> out;
  for (const auto& [ccy, usd_per] : kSnapshotUsdPer) {
    double rebased = usd_per / b;
    // round to 6 decimal places, matching Python's round(v/b, 6)
    out[ccy] = std::round(rebased * 1e6) / 1e6;
  }
  return out;
}

std::unordered_map<std::string, double> load_rates_with_cache(const std::string& cache_json_path,
                                                                const std::string& base) {
  std::ifstream f(cache_json_path);
  if (f) {
    try {
      nlohmann::json data;
      f >> data;
      if (data.contains("base") && data["base"] == base && data.contains("rates") &&
          !data["rates"].empty()) {
        std::unordered_map<std::string, double> rates;
        for (auto& [ccy, rate] : data["rates"].items()) rates[ccy] = rate.get<double>();
        return rates;
      }
    } catch (const nlohmann::json::exception&) {
      // matches Python's `except Exception: pass` -- fall through to snapshot
    }
  }
  return load_rates_snapshot(base);
}

}  // namespace bazaartalks::quant_core
