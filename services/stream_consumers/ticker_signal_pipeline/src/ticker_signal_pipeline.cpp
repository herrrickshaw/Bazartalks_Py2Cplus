#include "bazaartalks/services/ticker_signal_pipeline.hpp"

#include "bazaartalks/stats/technical_indicators.hpp"

#include <cmath>
#include <nlohmann/json.hpp>

namespace bazaartalks::services::ticker_signal_pipeline {

namespace {
double round_n(double x, int decimals) {
  double scale = std::pow(10.0, decimals);
  return std::round(x * scale) / scale;
}
}  // namespace

TickerSignalResult compute_ticker_signal(const std::string& ticker,
                                          const std::vector<double>& close) {
  if (close.size() < 60) {
    return TickerSignalResult{ticker, "NO_DATA", std::nullopt, std::nullopt};
  }

  using namespace bazaartalks::stats;
  std::size_t last = close.size() - 1;
  std::vector<double> rsi_v = rsi(close, 14);
  std::vector<double> hi60 = rolling_max(close, 60);

  double last_close = close[last];
  double rsi_last = rsi_v[last];
  std::string signal;
  if (last_close >= hi60[last]) {
    signal = "BREAKOUT";
  } else if (rsi_last < 30.0) {
    signal = "OVERSOLD";
  } else {
    signal = "NEUTRAL";
  }

  TickerSignalResult out;
  out.ticker = ticker;
  out.signal = signal;
  out.rsi = round_n(rsi_last, 1);
  out.close = round_n(last_close, 2);
  return out;
}

std::string to_json(const TickerSignalResult& result) {
  nlohmann::json j;
  j["ticker"] = result.ticker;
  j["signal"] = result.signal;
  if (result.rsi.has_value()) j["rsi"] = result.rsi.value();
  if (result.close.has_value()) j["close"] = result.close.value();
  return j.dump();
}

}  // namespace bazaartalks::services::ticker_signal_pipeline
