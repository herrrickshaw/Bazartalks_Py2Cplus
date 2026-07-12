#pragma once
// Port of stream_pipeline.py -- a Kafka work-queue: a producer publishes
// the ticker universe to `scan.tickers`, any number of consumer workers
// (same group) pull partitions in parallel, compute a per-ticker screen
// signal, and publish results to `scan.signals`.
//
// Scope boundary: `_universe()`'s Excel-file discovery and `produce()`/
// `consume()`/`demo()`'s Kafka produce/poll orchestration are data-
// plumbing (see src/main.cpp), untested here by design -- same boundary
// as every other phase's `main()`. What's ported and tested is
// `_signal()`'s actual screen classification, reusing the same RSI/
// rolling-max kernels already golden-tested in libs/stats.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::services::ticker_signal_pipeline {

struct TickerSignalResult {
  std::string ticker;
  std::string signal;  // "NO_DATA" | "BREAKOUT" | "OVERSOLD" | "NEUTRAL"
  std::optional<double> rsi;    // present unless signal == "NO_DATA"
  std::optional<double> close;  // present unless signal == "NO_DATA"
};

// Port of _signal(): "NO_DATA" if close.size() < 60 (MIN_BARS=60,
// implicit in the Python original via `len(df) < 60`); otherwise
// "BREAKOUT" if the last close is a new 60-bar high, else "OVERSOLD" if
// RSI(14) < 30, else "NEUTRAL". `rsi`/`close` are rounded to 1/2 decimals
// respectively, matching `round(float(rsi.iloc[-1]), 1)` /
// `round(float(last), 2)`.
TickerSignalResult compute_ticker_signal(const std::string& ticker,
                                          const std::vector<double>& close);

// Serializes a TickerSignalResult to the exact JSON shape _signal()
// returns (and consume() then json.dumps()'s onto the `scan.signals`
// topic): `{"ticker":..., "signal":...}` for NO_DATA (no rsi/close keys
// at all, matching `{"ticker": ticker, "signal": "NO_DATA"}`), or
// `{"ticker":..., "signal":..., "rsi":..., "close":...}` otherwise.
std::string to_json(const TickerSignalResult& result);

}  // namespace bazaartalks::services::ticker_signal_pipeline
