#include "bazaartalks/storage/warehouse.hpp"

#include <sstream>

namespace bazaartalks::storage {

namespace {

// SQLite result-DB attachment table: alias -> (path field, {view: table}).
// Mirrors warehouse.py's SQLITE_SOURCES dict verbatim.
struct SqliteSource {
  const char* alias;
  const std::optional<std::string> WarehousePaths::*path_field;
  const char* view;
  const char* table;
};

const SqliteSource kSqliteSources[] = {
    {"fund_db", &WarehousePaths::fundamentals_cache_db, "fundamentals", "fund"},
    {"dvmg_db", &WarehousePaths::dvm_global_db, "dvm_global", "dvm_global"},
    {"dvmc_db", &WarehousePaths::dvm_composite_db, "dvm_composite", "dvm_composite"},
    {"viab_db", &WarehousePaths::viability_summary_db, "viability", "market_screen_summary"},
};

}  // namespace

Warehouse::Warehouse(const std::string& duckdb_path, WarehousePaths paths)
    : db_(duckdb_path.c_str()), con_(db_), paths_(std::move(paths)) {}

void Warehouse::build() {
  con_.Query("INSTALL sqlite; LOAD sqlite;");

  if (!paths_.ohlc_parquet_dirs.empty()) {
    std::ostringstream glob_list;
    glob_list << "[";
    for (std::size_t i = 0; i < paths_.ohlc_parquet_dirs.size(); ++i) {
      if (i > 0) glob_list << ", ";
      glob_list << "'" << paths_.ohlc_parquet_dirs[i] << "/cleaned_long_*.parquet'";
    }
    glob_list << "]";
    con_.Query(
        "CREATE OR REPLACE VIEW ohlc AS "
        "SELECT Symbol AS ticker, Date, Open, High, Low, Close, Volume, "
        "regexp_extract(filename, 'cleaned_long_([A-Za-z]+)\\.parquet', 1) AS market "
        "FROM read_parquet(" +
        glob_list.str() + ", filename=true)");
  }

  if (paths_.companies_industry_parquet) {
    con_.Query("CREATE OR REPLACE VIEW companies AS SELECT * FROM read_parquet('" +
               *paths_.companies_industry_parquet + "')");
  }

  for (const auto& src : kSqliteSources) {
    const std::optional<std::string>& path = paths_.*(src.path_field);
    if (!path) continue;

    con_.Query(std::string("ATTACH IF NOT EXISTS '") + *path + "' AS " + src.alias +
               " (TYPE sqlite)");

    if (std::string(src.view) == "fundamentals") {
      // yfinance can store 'Infinity' as text -> coerce via TRY_CAST,
      // matching Python's fundamentals-view special case exactly.
      const std::string base_cols =
          "ticker, market, "
          "TRY_CAST(pe AS DOUBLE) pe, TRY_CAST(pb AS DOUBLE) pb, "
          "TRY_CAST(roe AS DOUBLE) roe, TRY_CAST(roa AS DOUBLE) roa, "
          "TRY_CAST(de AS DOUBLE) de, TRY_CAST(rev_growth AS DOUBLE) rev_growth, "
          "TRY_CAST(earn_growth AS DOUBLE) earn_growth, TRY_CAST(op_margin AS DOUBLE) op_margin, "
          "TRY_CAST(div_yield AS DOUBLE) div_yield, TRY_CAST(mktcap AS DOUBLE) mktcap, sector";
      std::string table_ref = std::string(src.alias) + "." + src.table;
      auto with_source = con_.Query("CREATE OR REPLACE VIEW fundamentals AS SELECT " + base_cols +
                                     ", source FROM " + table_ref);
      if (with_source->HasError()) {
        // Older fund tables built by fundamentals_global.py alone won't
        // have the additive `source` column yet -- fall back like Python.
        con_.Query("CREATE OR REPLACE VIEW fundamentals AS SELECT " + base_cols +
                   ", CAST(NULL AS VARCHAR) AS source FROM " + table_ref);
      }
    } else {
      con_.Query(std::string("CREATE OR REPLACE VIEW ") + src.view + " AS SELECT * FROM " +
                 src.alias + "." + src.table);
    }
  }
}

std::vector<std::string> Warehouse::tables() {
  auto result = con_.Query("SHOW TABLES");
  std::vector<std::string> out;
  for (auto& row : *result) {
    out.push_back(row.GetValue<std::string>(0));
  }
  return out;
}

duckdb::unique_ptr<duckdb::QueryResult> Warehouse::ticker_ohlc(const std::string& ticker,
                                                                const std::string& market,
                                                                int bars) {
  auto stmt = con_.Prepare(
      "SELECT Date, Open, High, Low, Close, Volume FROM ohlc "
      "WHERE ticker = ? AND market = ? ORDER BY Date DESC LIMIT ?");
  return stmt->Execute(ticker, market, bars);
}

duckdb::unique_ptr<duckdb::QueryResult> Warehouse::ticker_join(const std::string& view,
                                                                const std::string& ticker,
                                                                const std::string& market) {
  auto stmt = con_.Prepare("SELECT * FROM " + view + " WHERE ticker = ? AND market = ? LIMIT 1");
  return stmt->Execute(ticker, market);
}

duckdb::unique_ptr<duckdb::MaterializedQueryResult> Warehouse::query(const std::string& sql) {
  return con_.Query(sql);
}

}  // namespace bazaartalks::storage
