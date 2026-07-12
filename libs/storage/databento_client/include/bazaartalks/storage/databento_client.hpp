#pragma once
// C++ wrapper over databento-cpp's Historical client -- new capability, not a
// port (no Python original exists for order-book/tick/options data anywhere
// in this platform). See docs/MIGRATION_PLAN.md's Databento integration
// section for the full scoping rationale.
//
// MVP scope: historical OHLCV-1m, OHLCV-1d, and TBBO bars for US equities,
// stored into a dedicated SQLite db via the existing bt_sqlite_client.
// OHLCV-1d added alongside 1m/TBBO specifically because it's dramatically
// cheaper (real Databento pricing: ~$1.40/symbol for 3.3 years of 1-minute
// bars vs. a small fraction of that for daily bars over the same window)
// and directly comparable to the rest of this platform's daily-OHLC-based
// scanners, unlike minute/tick granularity which nothing else here consumes
// yet. Deferred: full order-book (MBP-10/MBO), OPRA options, live
// streaming, and wiring into bt_serve_api/bt_pipeline_cli's DuckDB views.
//
// This is a PAID, METERED API -- unlike every other data source in this
// repo, a mistake here costs real money. Every fetch_and_store() call:
//   1. checks local coverage() first, narrowing the request to the missing
//      delta (the single most effective guard against re-billing a re-run);
//   2. calls Databento's own MetadataGetCost() for that (narrowed) delta;
//   3. runs the estimate through evaluate_cost_gate(), a pure/network-free
//      decision function (unit-tested directly, see tests/); a request
//      whose estimate exceeds cost_threshold_usd_ is refused UNLESS the
//      caller passes back the exact estimated amount via confirmed_cost_usd
//      (not a bare boolean -- see evaluate_cost_gate's own comment for why);
//   4. logs every real (billed) call to the local spend_ledger table.
//
// DATABENTO_API_KEY is a SECRET, not config -- unlike this repo's one other
// C++ env-var precedent (KAFKA_BOOTSTRAP, a non-secret value with a safe
// hardcoded fallback), there is no safe fallback for an API key. This class
// delegates entirely to databento::HistoricalBuilder::SetKeyFromEnv(), which
// already throws a clear databento::Exception if the env var is unset --
// deliberately not re-implemented here so there's exactly one place (inside
// the vendor library itself) that decides what "unset" means. The key value
// is never logged, never written to any file by this module, and never
// appears in an exception message.

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace bazaartalks::storage {

enum class DatabentoSchema { Ohlcv1M, Ohlcv1D, Tbbo };

std::string to_string(DatabentoSchema schema);

// Decoded OHLCV-1m record (databento::OhlcvMsg, prices converted out of
// Databento's fixed-point int64 representation into human-readable doubles).
struct DatabentoBar {
  std::string symbol;
  std::int64_t ts_event_ns = 0;
  double open = 0.0;
  double high = 0.0;
  double low = 0.0;
  double close = 0.0;
  std::uint64_t volume = 0;
};

// Decoded TBBO record (databento::Mbp1Msg -- a trade paired with the best
// bid/offer immediately before it).
struct DatabentoQuote {
  std::string symbol;
  std::int64_t ts_event_ns = 0;
  double price = 0.0;
  std::uint32_t size = 0;
  double bid_px = 0.0;
  double ask_px = 0.0;
  std::uint32_t bid_sz = 0;
  std::uint32_t ask_sz = 0;
};

// What's already in the local db for a (symbol, schema) pair -- mirrors
// MarketStore::Coverage's shape (libs/storage/cassandra_client) so the same
// "check what's already there before fetching" idiom reads the same way
// across both storage clients.
struct DatabentoCoverage {
  std::string min_date;  // "YYYY-MM-DD", inclusive
  std::string max_date;  // "YYYY-MM-DD", inclusive
  std::int64_t n = 0;
};

struct CostGateDecision {
  bool allowed = false;
  std::string reason;
};

// Pure, network-free cost-gate logic -- the one piece of this module that
// bears real financial risk if wrong, and therefore the one piece that's
// unit-tested directly without touching the network (see
// databento_client_tests.cpp). Deliberately NOT a method on DatabentoClient
// so a test can exercise every branch (under threshold / over threshold no
// override / over threshold with a matching override / over threshold with
// a stale-or-wrong override) without constructing a real client at all.
//
// `confirmed_cost_usd` must equal `estimated_cost_usd` (within a small
// relative tolerance, to absorb float noise -- not a wide band) to count as
// a valid override. A bare "--force"-style boolean would let a script or a
// contributor's shell alias silently defeat the gate every time; requiring
// the actual number forces whoever is running the override to have looked
// at the real estimate first.
CostGateDecision evaluate_cost_gate(double estimated_cost_usd, double threshold_usd,
                                     std::optional<double> confirmed_cost_usd);

class DatabentoCostExceeded : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class DatabentoClient {
 public:
  // `bars_db_path`: a dedicated SQLite file (e.g. databento_bars.db),
  // created/migrated via CREATE TABLE IF NOT EXISTS on construction, same
  // idempotent-DDL-in-constructor convention as MarketStore. `cost_threshold_usd`
  // defaults to a conservative $1.00 -- deliberately low; real ingest runs are
  // expected to pass an explicit, considered threshold.
  explicit DatabentoClient(std::string bars_db_path, double cost_threshold_usd = 1.0);
  ~DatabentoClient();
  DatabentoClient(const DatabentoClient&) = delete;

  // What's already stored locally for this (symbol, schema) -- nullopt if
  // nothing's been fetched yet.
  std::optional<DatabentoCoverage> coverage(const std::string& symbol, DatabentoSchema schema);

  // Real network call to Databento's MetadataGetCost. Does not fetch or
  // store any data, and itself has no cost (metadata calls are free) --
  // safe to call as many times as needed before deciding whether to
  // actually fetch.
  double estimate_cost(const std::string& dataset, const std::string& symbol,
                       DatabentoSchema schema, const std::string& start_date,
                       const std::string& end_date);

  // Narrows [start_date, end_date) to whatever isn't already covered
  // locally, estimates the cost of that (possibly empty, in which case this
  // returns 0 without any network call at all) delta, runs it through
  // evaluate_cost_gate(), and either throws DatabentoCostExceeded or
  // proceeds to fetch + store the records and append one spend_ledger row.
  // Returns the number of new rows written (0 if the request was already
  // fully covered).
  std::size_t fetch_and_store(const std::string& dataset, const std::string& symbol,
                              DatabentoSchema schema, const std::string& start_date,
                              const std::string& end_date,
                              std::optional<double> confirmed_cost_usd = std::nullopt);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace bazaartalks::storage
