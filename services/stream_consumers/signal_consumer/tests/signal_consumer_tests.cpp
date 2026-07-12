// python3 cross-checked against kafka_signal_consumer.py's parse block and
// collections.Counter.most_common()'s exact tie-breaking rule (stable
// sort by count descending, ties in first-seen order) -- see the commit
// message / PR description.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/services/signal_consumer.hpp"

using namespace bazaartalks::services::signal_consumer;

TEST_CASE("parse_signal matches the Python original's parse-or-skip / UNKNOWN-default "
          "contract",
          "[signal_consumer]") {
  auto ok = parse_signal(R"({"ticker":"AAPL","signal":"BREAKOUT"})");
  REQUIRE(ok.has_value());
  CHECK(*ok == "BREAKOUT");

  auto missing_signal = parse_signal(R"({"ticker":"AAPL"})");
  REQUIRE(missing_signal.has_value());
  CHECK(*missing_signal == "UNKNOWN");  // `rec.get("signal", "UNKNOWN")`

  CHECK(parse_signal("not json").has_value() == false);
}

TEST_CASE("SignalWindowAggregator.flush() matches Counter.most_common()'s count-descending "
          "order",
          "[signal_consumer]") {
  SignalWindowAggregator agg(5.0, 1000.0);
  agg.add("BREAKOUT");
  agg.add("OVERSOLD");
  agg.add("NEUTRAL");
  agg.add("NEUTRAL");
  agg.add("NEUTRAL");
  agg.add("BREAKOUT");

  auto rows = agg.flush();
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].first == "NEUTRAL");
  CHECK(rows[0].second == 3);
  CHECK(rows[1].first == "BREAKOUT");
  CHECK(rows[1].second == 2);
  CHECK(rows[2].first == "OVERSOLD");
  CHECK(rows[2].second == 1);
}

// python3: Counter() with 'B' inserted before 'A' before 'C', where A and
// C both end at count 2 -- most_common() keeps A ahead of C because A was
// FIRST-SEEN before C, not because of any alphabetical tie-break.
TEST_CASE("SignalWindowAggregator ties preserve first-seen order, not insertion-sorted "
          "or alphabetical order",
          "[signal_consumer]") {
  SignalWindowAggregator agg(5.0, 1000.0);
  agg.add("B");
  agg.add("A");
  agg.add("C");
  agg.add("C");
  agg.add("A");

  auto rows = agg.flush();
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].first == "A");  // count 2, first-seen before C
  CHECK(rows[0].second == 2);
  CHECK(rows[1].first == "C");  // count 2, first-seen after A
  CHECK(rows[1].second == 2);
  CHECK(rows[2].first == "B");  // count 1
  CHECK(rows[2].second == 1);
}

TEST_CASE("SignalWindowAggregator window-elapsed and flush-empty behavior",
          "[signal_consumer]") {
  SignalWindowAggregator agg(5.0, 1000.0);
  CHECK(agg.flush().empty());
  CHECK(agg.should_flush(1004.9) == false);
  CHECK(agg.should_flush(1005.0) == true);

  agg.add("X");
  agg.reset(1005.0);
  CHECK(agg.flush().empty());
}
