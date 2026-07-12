#pragma once
// Port of market_store.py -- the Cassandra-backed OHLC cache. Cassandra and
// the Kafka CDC publish are MANDATORY here, not optional fallbacks, exactly
// as in the Python original: every write raises rather than silently
// degrading to "no cache" behavior. This client does NOT include
// market_store.py's `cached_download()` orchestration (the part that calls
// yfinance on a cache miss) -- yfinance stays in the Python sidecar per the
// migration plan, so this client only covers the storage primitives
// (`coverage`, `put_ohlc`, `get_ohlc`) that C++ compute modules read from
// directly. Freshness-check-and-refetch policy remains a sidecar concern.
//
// Schema (keyspace `market`, created if missing):
//   ohlc_bars(ticker text, d date, o,h,l,c,v double, PRIMARY KEY (ticker, d))

#include <date/date.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace bazaartalks::storage {

class MarketStoreUnavailable : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct OhlcBar {
  date::year_month_day d;
  double o = 0.0, h = 0.0, l = 0.0, c = 0.0, v = 0.0;
};

struct Coverage {
  date::year_month_day min_date;
  date::year_month_day max_date;
  std::int64_t n_bars = 0;
};

class MarketStore {
 public:
  // Connects to Cassandra and ensures the `market` keyspace/`ohlc_bars`
  // table exist, mirroring _connect(). Throws MarketStoreUnavailable
  // immediately (not lazily on first query) if the cluster can't be
  // reached within `connect_timeout` -- callers must handle this
  // explicitly rather than getting silent no-cache behavior, per the
  // Python original's own design contract.
  explicit MarketStore(const std::string& contact_points = "127.0.0.1", int port = 9042,
                        std::chrono::milliseconds connect_timeout = std::chrono::milliseconds(4000),
                        const std::string& kafka_bootstrap = "localhost:9092");
  ~MarketStore();

  MarketStore(const MarketStore&) = delete;
  MarketStore& operator=(const MarketStore&) = delete;

  // (min_date, max_date, n_bars) held for a ticker, or nullopt if nothing
  // is cached yet. Distinct from MarketStoreUnavailable, which is reserved
  // for "Cassandra itself is unreachable" (thrown from the constructor,
  // not from here).
  std::optional<Coverage> coverage(const std::string& ticker);

  // Writes bars for a ticker (idempotent upsert on the (ticker, d) primary
  // key) and publishes the mandatory CDC mutation event to Kafka. Throws
  // bazaartalks::streaming::CdcPublishError if the Kafka publish fails --
  // this is NOT caught/swallowed here, matching put_ohlc()'s contract that
  // both the write and the CDC event are mandatory.
  void put_ohlc(const std::string& ticker, const std::vector<OhlcBar>& bars);

  std::vector<OhlcBar> get_ohlc(const std::string& ticker);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace bazaartalks::storage
