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
