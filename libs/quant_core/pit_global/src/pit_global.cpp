#include "bazaartalks/quant_core/pit_global.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace bazaartalks::quant_core {

namespace {
const std::unordered_map<std::string, std::string>& cost_bucket() {
  static const std::unordered_map<std::string, std::string> kBucket = {
      {"US", "US"}, {"JP", "Japan"}, {"KR", "Korea"}, {"DE", "Europe"}, {"EU", "Europe"},
      {"FI", "Europe"}, {"DK", "Europe"}, {"CH", "Europe"}, {"SE", "Europe"}, {"UK", "Europe"},
  };
  return kBucket;
}

const std::unordered_map<std::string, double>& costs_pct() {
  static const std::unordered_map<std::string, double> kCosts = {
      {"India", 0.30}, {"US", 0.10}, {"Japan", 0.20}, {"Korea", 0.25}, {"Europe", 0.40},
  };
  return kCosts;
}

double mean(const std::vector<double>& v) {
  double sum = 0.0;
  for (double x : v) sum += x;
  return sum / static_cast<double>(v.size());
}
}  // namespace

double market_cost(const std::string& market) {
  const auto& bucket = cost_bucket();
  auto bit = bucket.find(market);
  std::string bucket_name = bit != bucket.end() ? bit->second : "";
  const auto& costs = costs_pct();
  auto cit = costs.find(bucket_name);
  return cit != costs.end() ? cit->second : 0.30;
}

bool is_durable(double roe, double de) { return roe > 12.0 && de < 1.5; }

std::string base_symbol_upper(const std::string& ticker) {
  std::string base = ticker.substr(0, ticker.find('.'));
  std::transform(base.begin(), base.end(), base.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return base;
}

std::vector<MarketArmResult> run_market(const std::string& market,
                                        const std::vector<TickerHistory>& tickers, bool overlay,
                                        const std::set<std::string>& durable,
                                        std::size_t darvas_lookback, std::size_t min_lookback) {
  if (tickers.size() < 20) return {};

  double cost = market_cost(market);
  auto rebal = monthly_rebalance_dates(tickers, min_lookback);

  std::vector<double> darvas_rets, bench_rets, durable_rets;
  for (std::size_t i = 0; i + 1 < rebal.size(); ++i) {
    auto rows = period_returns(tickers, rebal[i], rebal[i + 1], darvas_lookback);
    if (rows.empty()) continue;

    std::vector<double> held, breakout, dur_breakout;
    for (const auto& r : rows) {
      held.push_back(r.forward_return_pct);
      if (r.is_darvas_breakout) {
        breakout.push_back(r.forward_return_pct);
        if (overlay && durable.count(base_symbol_upper(r.ticker))) {
          dur_breakout.push_back(r.forward_return_pct);
        }
      }
    }
    bench_rets.push_back(mean(held) - cost);
    if (!breakout.empty()) darvas_rets.push_back(mean(breakout) - cost);
    if (overlay && !dur_breakout.empty()) durable_rets.push_back(mean(dur_breakout) - cost);
  }

  std::vector<MarketArmResult> out;
  out.push_back({market, "A_darvas", compute_metrics(darvas_rets)});
  out.push_back({market, "BENCH_all", compute_metrics(bench_rets)});
  if (overlay) out.push_back({market, "D_darvas_durable", compute_metrics(durable_rets)});
  return out;
}

}  // namespace bazaartalks::quant_core
