// Fixture parquet files are written via DuckDB's COPY TO (already a Phase 2
// dependency) rather than Arrow's own writer API, matching how
// duckdb_client's tests build fixtures -- keeps this test self-contained.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <duckdb.hpp>

#include <cmath>
#include <filesystem>

#include "bazaartalks/storage/wide_frame.hpp"

using namespace bazaartalks::storage;
using Catch::Approx;

namespace {
std::string write_fixture(const std::filesystem::path& path) {
  duckdb::DuckDB db(nullptr);
  duckdb::Connection con(db);
  // Symbol B is missing a 2026-01-05 row (to exercise NaN-fill), and
  // AAPL's 2026-01-02 Close appears twice with different values to
  // exercise aggfunc="last" (the second/later row must win).
  con.Query(
      "COPY (SELECT CAST(Date AS DATE) AS Date, Symbol, CAST(Close AS DOUBLE) AS Close FROM (VALUES "
      "('2026-01-02', 'AAPL', 100.0), "
      "('2026-01-02', 'AAPL', 999.0), "  // duplicate (date,symbol) -- last wins
      "('2026-01-05', 'AAPL', 101.0), "
      "('2026-01-02', 'MSFT', 200.0), "
      "('2026-01-05', 'MSFT', 202.0)"
      ") AS t(Date, Symbol, Close)) TO '" +
      path.string() + "' (FORMAT PARQUET)");
  return path.string();
}
}  // namespace

TEST_CASE("pivot_long_parquet returns nullopt for a missing file", "[parquet_io]") {
  auto result = pivot_long_parquet("/nonexistent/path/cleaned_long_ZZ.parquet", "Close");
  CHECK_FALSE(result.has_value());
}

TEST_CASE("pivot_long_parquet pivots to a Date x Symbol matrix with aggfunc=last semantics",
          "[parquet_io]") {
  auto tmp_dir = std::filesystem::temp_directory_path() / "bt_parquet_io_test";
  std::filesystem::create_directories(tmp_dir);
  auto fixture = write_fixture(tmp_dir / "cleaned_long_US.parquet");

  auto frame = pivot_long_parquet(fixture, "Close");
  REQUIRE(frame.has_value());
  REQUIRE(frame->dates.size() == 2);
  REQUIRE(frame->symbols.size() == 2);
  CHECK(frame->symbols[0] == "AAPL");
  CHECK(frame->symbols[1] == "MSFT");

  // Duplicate (2026-01-02, AAPL) row -> the LAST value (999.0) wins, not
  // the first (100.0), matching pandas pivot_table(aggfunc="last").
  auto aapl_d1 = frame->at(0, 0);
  REQUIRE(aapl_d1.has_value());
  CHECK(*aapl_d1 == Approx(999.0));

  CHECK(*frame->at(1, 0) == Approx(101.0));  // AAPL 2026-01-05
  CHECK(*frame->at(0, 1) == Approx(200.0));  // MSFT 2026-01-02
  CHECK(*frame->at(1, 1) == Approx(202.0));  // MSFT 2026-01-05

  std::filesystem::remove_all(tmp_dir);
}
