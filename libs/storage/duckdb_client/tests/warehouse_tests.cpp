// Uses an in-memory DuckDB instance and writes a small fixture parquet
// file via DuckDB's own COPY TO (no dependency on Phase 2's parquet_io or
// external Python-written fixtures for this smoke test) to exercise
// build()'s view-attachment logic end-to-end.
#include <catch2/catch_test_macros.hpp>
#include <duckdb.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "bazaartalks/storage/warehouse.hpp"

using namespace bazaartalks::storage;

TEST_CASE("Warehouse::build with no configured paths is a no-op besides INSTALL/LOAD",
          "[duckdb]") {
  Warehouse wh(":memory:", WarehousePaths{});
  REQUIRE_NOTHROW(wh.build());
  auto tbls = wh.tables();
  CHECK(tbls.empty());

  auto result = wh.query("SELECT 1 AS one");
  REQUIRE_FALSE(result->HasError());
  REQUIRE(result->RowCount() == 1);
  CHECK(result->GetValue<int64_t>(0, 0) == 1);
}

TEST_CASE("Warehouse::build attaches an ohlc view over a fixture parquet directory",
          "[duckdb]") {
  auto tmp_dir = std::filesystem::temp_directory_path() / "bt_warehouse_test";
  std::filesystem::create_directories(tmp_dir);
  auto fixture_path = (tmp_dir / "cleaned_long_US.parquet").string();

  {
    // Build the fixture with a throwaway in-memory DuckDB instance writing
    // via COPY TO, matching the exact column names warehouse.py's `ohlc`
    // view expects (Symbol, Date, Open, High, Low, Close, Volume).
    duckdb::DuckDB fixture_db(nullptr);
    duckdb::Connection fixture_con(fixture_db);
    fixture_con.Query(
        "COPY (SELECT 'AAPL' AS Symbol, DATE '2026-01-02' AS Date, "
        "100.0 AS Open, 102.0 AS High, 99.0 AS Low, 101.0 AS Close, 1000.0 AS Volume) "
        "TO '" +
        fixture_path + "' (FORMAT PARQUET)");
  }
  REQUIRE(std::filesystem::exists(fixture_path));

  WarehousePaths paths;
  paths.ohlc_parquet_dirs.push_back(tmp_dir.string());
  Warehouse wh(":memory:", paths);
  wh.build();

  auto tbls = wh.tables();
  CHECK(std::find(tbls.begin(), tbls.end(), "ohlc") != tbls.end());

  auto result = wh.query("SELECT ticker, market, Close FROM ohlc");
  REQUIRE_FALSE(result->HasError());
  REQUIRE(result->RowCount() == 1);
  CHECK(result->GetValue(0, 0).ToString() == "AAPL");
  CHECK(result->GetValue(1, 0).ToString() == "US");

  std::filesystem::remove_all(tmp_dir);
}
