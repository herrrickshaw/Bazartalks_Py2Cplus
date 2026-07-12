// Network-free unit tests. Two things are tested here:
//   1. evaluate_cost_gate() -- the financial-risk-bearing logic, and
//      therefore the most important thing in this module to get right.
//      Pure function, no DatabentoClient/network involved at all.
//   2. DatabentoClient's schema creation + coverage()/spend-ledger logic --
//      these only touch a local SQLite file, never the network, since
//      coverage() is a pure local read and the constructor's DDL is
//      idempotent CREATE TABLE IF NOT EXISTS.
//
// DBN wire-format decoding (fetch_and_store()'s record parsing) is
// deliberately NOT tested here -- it needs a real recorded Databento
// response, not hand-synthesized DBN bytes (see docs/MIGRATION_PLAN.md's
// Databento integration section for why). That's tests/fixtures/ +
// databento_client_integration_tests.cpp instead.
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <utility>
#include <vector>

#include "bazaartalks/storage/databento_client.hpp"

using namespace bazaartalks::storage;

namespace {
std::string temp_db_path(const char* name) {
  return (std::filesystem::temp_directory_path() / name).string();
}
}  // namespace

TEST_CASE("evaluate_cost_gate allows a request under the threshold with no override needed",
          "[databento][cost_gate]") {
  auto decision = evaluate_cost_gate(/*estimated=*/0.05, /*threshold=*/1.0, std::nullopt);
  CHECK(decision.allowed);
}

TEST_CASE("evaluate_cost_gate blocks a request over the threshold with no override",
          "[databento][cost_gate]") {
  auto decision = evaluate_cost_gate(/*estimated=*/5.00, /*threshold=*/1.0, std::nullopt);
  CHECK_FALSE(decision.allowed);
  CHECK(decision.reason.find("threshold") != std::string::npos);
}

TEST_CASE(
    "evaluate_cost_gate allows an over-threshold request when the override matches the estimate",
    "[databento][cost_gate]") {
  auto decision = evaluate_cost_gate(/*estimated=*/5.00, /*threshold=*/1.0,
                                     /*confirmed=*/5.00);
  CHECK(decision.allowed);
}

TEST_CASE(
    "evaluate_cost_gate REJECTS an over-threshold request when the override doesn't match "
    "the real estimate -- a stale or guessed confirmation must not slip through",
    "[databento][cost_gate]") {
  auto decision = evaluate_cost_gate(/*estimated=*/5.00, /*threshold=*/1.0,
                                     /*confirmed=*/2.50);
  CHECK_FALSE(decision.allowed);
  CHECK(decision.reason.find("does not match") != std::string::npos);
}

TEST_CASE("evaluate_cost_gate tolerates small float noise in a matching override, not a genuinely "
          "different amount",
          "[databento][cost_gate]") {
  // Within tolerance (rounding from a displayed estimate) -- allowed.
  CHECK(evaluate_cost_gate(5.001, 1.0, 5.00).allowed);
  // Clearly a different number -- rejected, not silently accepted.
  CHECK_FALSE(evaluate_cost_gate(5.00, 1.0, 4.50).allowed);
}

TEST_CASE("evaluate_cost_gate at exactly the threshold is allowed (inclusive boundary)",
          "[databento][cost_gate]") {
  CHECK(evaluate_cost_gate(1.0, 1.0, std::nullopt).allowed);
}

TEST_CASE("DatabentoClient creates its schema idempotently and coverage() is nullopt before "
          "anything is stored",
          "[databento][client]") {
  auto path = temp_db_path("bt_databento_client_test1.db");
  std::remove(path.c_str());

  DatabentoClient client(path, /*cost_threshold_usd=*/1.0);
  CHECK_FALSE(client.coverage("AAPL", DatabentoSchema::Ohlcv1M).has_value());
  CHECK_FALSE(client.coverage("AAPL", DatabentoSchema::Ohlcv1D).has_value());
  CHECK_FALSE(client.coverage("AAPL", DatabentoSchema::Tbbo).has_value());

  // Constructing a second client against the same file must not throw
  // (CREATE TABLE IF NOT EXISTS, not CREATE TABLE) -- matches
  // MarketStore's idempotent-DDL-in-constructor convention.
  CHECK_NOTHROW(DatabentoClient(path, 1.0));

  std::remove(path.c_str());
}

TEST_CASE("to_string(DatabentoSchema) matches the table names used internally",
          "[databento][client]") {
  CHECK(to_string(DatabentoSchema::Ohlcv1M) == "ohlcv_1m");
  CHECK(to_string(DatabentoSchema::Ohlcv1D) == "ohlcv_1d");
  CHECK(to_string(DatabentoSchema::Tbbo) == "tbbo");
}

// Regression coverage for the multi-publisher DBEQ.BASIC shape described in
// benchmarks/pipeline_cutover/databento_client_comparison.md's Finding #2:
// three venues reporting the same (symbol, ts_event_ns) with different
// volumes. dedupe_by_max_volume() must keep exactly the highest-volume row,
// not whichever one happened to be encountered first.
TEST_CASE("dedupe_by_max_volume keeps the highest-volume row per ts_event_ns",
          "[databento][dedupe]") {
  // Mirrors the confirmed real-world MSFT case: stored row (publisher 40,
  // vol 15,072) was NOT the max-volume row (publisher 41, vol 969,305).
  std::vector<DatabentoBar> bars = {
      {"MSFT", /*ts=*/1000, 385.09, 385.10, 385.00, 385.09, /*vol=*/15'072},
      {"MSFT", /*ts=*/1000, 385.09, 385.12, 384.95, 385.09, /*vol=*/468'339},
      {"MSFT", /*ts=*/1000, 385.08, 385.15, 384.90, 385.08, /*vol=*/969'305},
  };

  auto deduped = dedupe_by_max_volume(std::move(bars));

  REQUIRE(deduped.size() == 1);
  CHECK(deduped[0].symbol == "MSFT");
  CHECK(deduped[0].ts_event_ns == 1000);
  CHECK(deduped[0].volume == 969'305);
  CHECK(deduped[0].close == 385.08);
}

TEST_CASE("dedupe_by_max_volume treats distinct ts_event_ns as distinct bars",
          "[databento][dedupe]") {
  // AAPL, SPY on different days/timestamps -- each publisher-triple must
  // collapse independently, not get merged across timestamps or symbols.
  std::vector<DatabentoBar> bars = {
      {"AAPL", /*ts=*/1000, 315.04, 316.0, 314.0, 315.04, /*vol=*/865'253},
      {"AAPL", /*ts=*/1000, 315.29, 316.0, 314.5, 315.29, /*vol=*/62'493},
      {"AAPL", /*ts=*/1000, 315.14, 316.0, 314.0, 315.14, /*vol=*/1'326'282},
      {"SPY", /*ts=*/2000, 500.0, 501.0, 499.0, 500.0, /*vol=*/25'000},
      {"SPY", /*ts=*/2000, 500.5, 501.0, 499.0, 500.5, /*vol=*/1'270'000},
  };

  auto deduped = dedupe_by_max_volume(std::move(bars));

  REQUIRE(deduped.size() == 2);
  for (const auto& bar : deduped) {
    if (bar.symbol == "AAPL") {
      CHECK(bar.ts_event_ns == 1000);
      CHECK(bar.volume == 1'326'282);
      CHECK(bar.close == 315.14);
    } else {
      REQUIRE(bar.symbol == "SPY");
      CHECK(bar.volume == 1'270'000);
      CHECK(bar.close == 500.5);
    }
  }
}

TEST_CASE("dedupe_by_max_volume is a no-op for already-unique ts_event_ns values",
          "[databento][dedupe]") {
  std::vector<DatabentoBar> bars = {
      {"SPY", /*ts=*/1000, 500.0, 501.0, 499.0, 500.5, /*vol=*/1'000},
      {"SPY", /*ts=*/2000, 501.0, 502.0, 500.0, 501.5, /*vol=*/2'000},
  };

  auto deduped = dedupe_by_max_volume(std::move(bars));

  REQUIRE(deduped.size() == 2);
}

TEST_CASE("dedupe_by_max_volume on an empty input returns an empty result",
          "[databento][dedupe]") {
  CHECK(dedupe_by_max_volume({}).empty());
}
