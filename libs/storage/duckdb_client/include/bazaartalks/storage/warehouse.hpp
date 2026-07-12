#pragma once
// Port of warehouse.py -- the DuckDB analytical/serving layer that attaches
// SQLite result DBs and reads parquet globs as SQL views (ohlc, companies,
// fundamentals, dvm_global, dvm_composite, viability). DuckDB's C++ API is
// used in-process (no client/server hop), which is a near-1:1 port since
// DuckDB itself is written in C++ -- Python's `duckdb.connect()` is a thin
// binding over the same engine this links against directly.
//
// Paths (the external `cache_seed` parquet directory, the SQLite result DB
// files) are constructor parameters here rather than hardcoded like the
// Python original's `~/Downloads/code/python_files/cache_seed` -- the C++
// core treats these as the file-based storage contract with the Python
// sidecar (see the migration plan's Python Boundary section), not
// environment-specific paths baked into the binary.

#include <duckdb.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::storage {

struct WarehousePaths {
  // Directories to glob for `cleaned_long_*.parquet` (unioned, like the
  // Python original's external-seed + repo-local cache_seed_local split).
  std::vector<std::string> ohlc_parquet_dirs;
  std::optional<std::string> companies_industry_parquet;   // companies_industry.parquet
  std::optional<std::string> fundamentals_cache_db;         // fundamentals_cache.db
  std::optional<std::string> dvm_global_db;                 // dvm_global.db
  std::optional<std::string> dvm_composite_db;               // dvm_composite.db
  std::optional<std::string> viability_summary_db;           // viability_summary.db
};

class Warehouse {
 public:
  // `duckdb_path` matches Python's `market.duckdb` (or ":memory:" for an
  // in-memory instance, useful for tests).
  explicit Warehouse(const std::string& duckdb_path, WarehousePaths paths);

  // Port of build(): (re)creates every view whose backing file/glob
  // exists, skipping (not erroring on) missing ones -- same "skip what's
  // absent" convention as the Python original. Safe to call repeatedly
  // (CREATE OR REPLACE VIEW), matching how warehouse.py rebuilds views on
  // every run rather than assuming they persist.
  void build();

  // Returns the set of currently-built view/table names (SHOW TABLES),
  // used by ticker_detail() to skip absent views the same way Python's
  // `available` set does.
  std::vector<std::string> tables();

  // Port of ticker_detail(): single-ticker lookup across every view
  // currently built. Uses DuckDB's parameterized-query API (not string
  // interpolation) for the ticker/market/bars inputs, matching Python's
  // bound `?` parameters -- these can come from a request path/query
  // param in the eventual HTTP layer (Phase 8), so this is a security-
  // relevant detail to preserve, not just a style choice.
  duckdb::unique_ptr<duckdb::QueryResult> ticker_ohlc(const std::string& ticker,
                                                       const std::string& market, int bars);
  duckdb::unique_ptr<duckdb::QueryResult> ticker_join(const std::string& view,
                                                       const std::string& ticker,
                                                       const std::string& market);

  // Runs arbitrary SQL (port of `--sql`). Callers are responsible for not
  // passing untrusted predicates here without going through Phase 8's
  // validate_predicate equivalent first -- this method itself does no
  // allow-listing, matching warehouse.py's own `--sql`/`--filter` CLI
  // flags, which are trusted-operator-only entry points, not the
  // internet-facing `/filter` endpoint (that's serve.py's job).
  duckdb::unique_ptr<duckdb::MaterializedQueryResult> query(const std::string& sql);

 private:
  duckdb::DuckDB db_;
  duckdb::Connection con_;
  WarehousePaths paths_;
};

}  // namespace bazaartalks::storage
