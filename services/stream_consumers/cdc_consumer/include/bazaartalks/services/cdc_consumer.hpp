#pragma once
// Port of kafka_cdc_consumer.py -- a tumbling-window aggregator counting
// CDC mutation events (`ohlc.cdc` topic) per ticker. This is the file
// market_store.py's own CDC comment names as the default/mandatory
// consumer over flink_cdc_consumer.py (see the Python original's
// docstring for why: PyFlink proved fragile in this environment for
// reasons unrelated to the actual ~40-line aggregation logic itself).
//
// Scope boundary: the actual poll loop (subscribe, poll, KeyboardInterrupt
// handling, argparse CLI) is I/O orchestration, thin and untested here by
// design, matching every other phase's "main() is data-plumbing" boundary
// -- see src/main.cpp. What's ported and tested is the pure logic: parsing
// one raw CDC record, and the tumbling-window bookkeeping that decides
// when to flush and what the flushed rows look like.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bazaartalks::services::cdc_consumer {

struct CdcRecord {
  std::string ticker;
  std::int64_t n_bars = 0;
};

// Port of parse_cdc(): returns std::nullopt for ANY parse failure
// (malformed JSON, missing "ticker" key) -- never throws, matching the
// Python original's blanket `except Exception: return None`. A missing
// "n_bars" key defaults to 0, matching `rec.get("n_bars", 0)`.
std::optional<CdcRecord> parse_cdc(const std::string& raw);

// Tumbling-window ticker -> total-n_bars-mutated aggregator. Pure and
// clock-independent: callers supply an explicit "now" (seconds, any
// monotonic origin) rather than this class reading the wall clock itself
// -- this is what makes the window-boundary logic testable without a
// real Kafka broker or a real timer.
class CdcWindowAggregator {
 public:
  CdcWindowAggregator(double window_seconds, double now);

  void add(const std::string& ticker, std::int64_t n_bars);

  // True iff `now - window_start >= window_seconds`, matching the Python
  // loop's own flush condition.
  bool should_flush(double now) const;

  // (ticker, total_n_bars) pairs in ticker-ascending order, matching
  // `for ticker, n in sorted(counts.items())`. Empty if nothing was added
  // this window (`_flush()`'s own `if not counts: return` no-op).
  std::vector<std::pair<std::string, std::int64_t>> flush() const;

  // Starts a fresh window at `now` with an empty count map, matching
  // `counts = defaultdict(int); window_start = now`.
  void reset(double now);

 private:
  double window_start_;
  double window_seconds_;
  std::map<std::string, std::int64_t> counts_;
};

}  // namespace bazaartalks::services::cdc_consumer
