// Live, gated integration test -- a SEPARATE test binary from
// databento_client_tests.cpp specifically so a normal unit-test run never
// links network code at all (mirrors cdc_consumer_integration_tests.cpp's
// structural precedent of one binary per "needs real infra" boundary).
//
// Skips/no-ops entirely unless DATABENTO_API_KEY is set in the environment.
// When it DOES run, it goes through the exact same cost-gate path as any
// other call (fetch_and_store()), with the smallest, cheapest possible
// real query this module supports: ONE US equity symbol (SPY, maximally
// liquid), ONE calendar day, the OHLCV-1m schema (aggregated bars, not raw
// ticks) against DBEQ.BASIC (Databento's basic-tier US equities dataset).
// Per Databento's own metered pricing this is expected to cost a small
// fraction of a cent -- MetadataGetCost() is asserted to return a real,
// finite, non-negative number (not hardcoded to a specific value, since
// Databento's own pricing can change) before anything is actually fetched,
// and the test's own cost_threshold_usd is set generously above what this
// specific tiny query could plausibly cost, so it exercises the gate's
// "allowed, no override needed" path rather than requiring one.
//
// This is also where tests/fixtures/ohlcv_1m_spy_sample.dbn gets captured
// from (once, by hand, not by this test) -- see that file's own README
// entry for how to regenerate it if it's ever missing.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "bazaartalks/storage/databento_client.hpp"

using namespace bazaartalks::storage;

TEST_CASE("DatabentoClient fetches a real, tiny OHLCV-1m sample end-to-end via the live API",
          "[databento][live]") {
  const char* key = std::getenv("DATABENTO_API_KEY");
  if (key == nullptr) {
    WARN("DATABENTO_API_KEY not set -- skipping live Databento integration test");
    return;
  }

  auto path = (std::filesystem::temp_directory_path() / "bt_databento_live_test.db").string();
  std::remove(path.c_str());

  // Generous threshold: this specific query (1 symbol, 1 day, 1-minute bars)
  // is expected to cost a small fraction of a cent; $0.50 leaves headroom
  // without disabling the gate's real purpose for anything larger.
  DatabentoClient client(path, /*cost_threshold_usd=*/0.50);

  const std::string dataset = "DBEQ.BASIC";
  const std::string symbol = "SPY";
  const std::string start = "2026-07-01";
  const std::string end = "2026-07-02";

  double cost = client.estimate_cost(dataset, symbol, DatabentoSchema::Ohlcv1M, start, end);
  CHECK(cost >= 0.0);
  CHECK(std::isfinite(cost));

  std::size_t written = client.fetch_and_store(dataset, symbol, DatabentoSchema::Ohlcv1M, start, end);
  CHECK(written > 0);

  auto cov = client.coverage(symbol, DatabentoSchema::Ohlcv1M);
  REQUIRE(cov.has_value());
  CHECK(cov->n == static_cast<std::int64_t>(written));

  // Re-running the identical request must be a no-op (fully covered
  // already) -- the single most important real-world cost-safety property:
  // a retry after a crash must not re-bill.
  std::size_t written_again =
      client.fetch_and_store(dataset, symbol, DatabentoSchema::Ohlcv1M, start, end);
  CHECK(written_again == 0);

  std::remove(path.c_str());
}
