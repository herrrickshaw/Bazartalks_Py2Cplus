#pragma once
// Thin RAII wrapper over the SQLite3 C API, used by every module reading
// or writing the platform's SQLite result DBs (fundamentals_cache.db,
// edgar_facts.db, viability_summary.db, watchlist_store.py's VCRUD tables
// in Phase 8, etc). Deliberately minimal -- no ORM, matching the migration
// plan's assessment that these are "thin wrapper over SQLite" targets with
// no complex Python-specific logic worth abstracting away.

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace bazaartalks::storage {

class SqliteError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// A bindable/retrievable SQLite value: NULL, INTEGER, REAL, or TEXT (BLOB
// is not modeled -- none of the ported modules' schemas use it).
using SqliteValue = std::variant<std::nullptr_t, std::int64_t, double, std::string>;

class SqliteStatement {
 public:
  SqliteStatement(sqlite3* db, const std::string& sql);
  ~SqliteStatement();
  SqliteStatement(const SqliteStatement&) = delete;
  SqliteStatement(SqliteStatement&&) noexcept;

  // 1-indexed bind, matching sqlite3_bind_*'s own convention.
  void bind(int index, const SqliteValue& value);

  // Advances to the next row. Returns false when there are no more rows
  // (SQLITE_DONE); throws SqliteError on any other non-SQLITE_ROW result.
  bool step();

  // Resets the statement (e.g. to re-execute with new bindings) without
  // re-preparing it.
  void reset();

  SqliteValue column(int index) const;
  int column_count() const;

 private:
  sqlite3_stmt* stmt_ = nullptr;
};

class SqliteDb {
 public:
  explicit SqliteDb(const std::string& path);
  ~SqliteDb();
  SqliteDb(const SqliteDb&) = delete;

  // Runs a statement with no result rows expected (DDL, INSERT/UPDATE/
  // DELETE without needing the inserted row back).
  void execute(const std::string& sql);

  SqliteStatement prepare(const std::string& sql);

  sqlite3* handle() const { return db_; }

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace bazaartalks::storage
