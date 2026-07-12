#pragma once
// Port of kafka_signal_consumer.py -- a tumbling-window aggregator
// counting scan.signals events by signal type, the same plain-Kafka
// pattern as cdc_consumer replacing flink_screens.py.
//
// Scope boundary: the poll loop / CLI is data-plumbing (see src/main.cpp),
// untested here by design. What's ported and tested is parse_signal() and
// the window bookkeeping, including Counter.most_common()'s exact tie-
// breaking rule (stable sort by count descending, ties keep first-seen
// order) -- a naive "sort by count" reimplementation that doesn't
// preserve insertion order on ties would silently disagree with the
// Python original whenever two signals tie in a window.

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bazaartalks::services::signal_consumer {

// Port of the inline parse-or-skip block in run(): std::nullopt for a
// JSON parse failure (matching the bare `except Exception: pass`, which
// silently drops the record without even counting it as "seen"). A
// parseable record missing the "signal" key maps to "UNKNOWN", matching
// `rec.get("signal", "UNKNOWN")` -- this is NOT a parse failure.
std::optional<std::string> parse_signal(const std::string& raw);

class SignalWindowAggregator {
 public:
  SignalWindowAggregator(double window_seconds, double now);

  void add(const std::string& signal);

  bool should_flush(double now) const;

  // (signal, count) pairs matching Counter.most_common(): sorted by count
  // descending, ties broken by first-seen order within this window (NOT
  // alphabetical or insertion-into-a-sorted-container order).
  std::vector<std::pair<std::string, int>> flush() const;

  void reset(double now);

 private:
  double window_start_;
  double window_seconds_;
  std::vector<std::string> insertion_order_;
  std::unordered_map<std::string, int> counts_;
};

}  // namespace bazaartalks::services::signal_consumer
