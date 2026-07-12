// python3 cross-checked against the actual parse_cdc()/run() logic in
// kafka_cdc_consumer.py (see the commit message / PR description).
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/services/cdc_consumer.hpp"

using namespace bazaartalks::services::cdc_consumer;

TEST_CASE("parse_cdc matches the Python original's parse-or-None contract", "[cdc_consumer]") {
  auto ok = parse_cdc(R"({"ticker":"AAPL","n_bars":5})");
  REQUIRE(ok.has_value());
  CHECK(ok->ticker == "AAPL");
  CHECK(ok->n_bars == 5);

  auto missing_n_bars = parse_cdc(R"({"ticker":"AAPL"})");
  REQUIRE(missing_n_bars.has_value());
  CHECK(missing_n_bars->ticker == "AAPL");
  CHECK(missing_n_bars->n_bars == 0);  // `rec.get("n_bars", 0)` default

  CHECK(parse_cdc("not json").has_value() == false);
  CHECK(parse_cdc(R"({"n_bars":5})").has_value() == false);  // missing "ticker" key
}

TEST_CASE("CdcWindowAggregator accumulates n_bars per ticker within a window",
          "[cdc_consumer]") {
  CdcWindowAggregator agg(/*window_seconds=*/30.0, /*now=*/1000.0);
  agg.add("AAPL", 3);
  agg.add("MSFT", 1);
  agg.add("AAPL", 2);  // same ticker, second event this window -> accumulates

  CHECK(agg.should_flush(1010.0) == false);  // 10s elapsed, window is 30s
  CHECK(agg.should_flush(1030.0) == true);   // exactly at the boundary

  auto rows = agg.flush();
  // sorted(counts.items()) -- ticker-ascending, AAPL before MSFT.
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].first == "AAPL");
  CHECK(rows[0].second == 5);
  CHECK(rows[1].first == "MSFT");
  CHECK(rows[1].second == 1);
}

TEST_CASE("CdcWindowAggregator.flush() is empty when nothing was added this window",
          "[cdc_consumer]") {
  CdcWindowAggregator agg(30.0, 1000.0);
  CHECK(agg.flush().empty());  // matches `_flush()`'s `if not counts: return` no-op
}

TEST_CASE("CdcWindowAggregator.reset starts a fresh window with cleared counts",
          "[cdc_consumer]") {
  CdcWindowAggregator agg(30.0, 1000.0);
  agg.add("AAPL", 5);
  agg.reset(1030.0);
  CHECK(agg.flush().empty());
  CHECK(agg.should_flush(1030.0) == false);  // window restarted at 1030
  CHECK(agg.should_flush(1060.0) == true);
}
