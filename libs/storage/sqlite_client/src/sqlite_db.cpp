#include "bazaartalks/storage/sqlite_db.hpp"

#include <sqlite3.h>

#include <utility>

namespace bazaartalks::storage {

SqliteDb::SqliteDb(const std::string& path) {
  int rc = sqlite3_open(path.c_str(), &db_);
  if (rc != SQLITE_OK) {
    std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown error";
    if (db_) sqlite3_close(db_);
    db_ = nullptr;
    throw SqliteError("failed to open SQLite database '" + path + "': " + msg);
  }
}

SqliteDb::~SqliteDb() {
  if (db_) sqlite3_close(db_);
}

void SqliteDb::execute(const std::string& sql) {
  char* errmsg = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    std::string msg = errmsg ? errmsg : "unknown error";
    sqlite3_free(errmsg);
    throw SqliteError("SQLite execute failed: " + msg + " (sql: " + sql + ")");
  }
}

SqliteStatement SqliteDb::prepare(const std::string& sql) { return SqliteStatement(db_, sql); }

SqliteStatement::SqliteStatement(sqlite3* db, const std::string& sql) {
  int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
  if (rc != SQLITE_OK) {
    std::string msg = sqlite3_errmsg(db);
    throw SqliteError("failed to prepare statement: " + msg + " (sql: " + sql + ")");
  }
}

SqliteStatement::~SqliteStatement() {
  if (stmt_) sqlite3_finalize(stmt_);
}

SqliteStatement::SqliteStatement(SqliteStatement&& other) noexcept : stmt_(other.stmt_) {
  other.stmt_ = nullptr;
}

void SqliteStatement::bind(int index, const SqliteValue& value) {
  int rc = SQLITE_OK;
  if (std::holds_alternative<std::nullptr_t>(value)) {
    rc = sqlite3_bind_null(stmt_, index);
  } else if (std::holds_alternative<std::int64_t>(value)) {
    rc = sqlite3_bind_int64(stmt_, index, std::get<std::int64_t>(value));
  } else if (std::holds_alternative<double>(value)) {
    rc = sqlite3_bind_double(stmt_, index, std::get<double>(value));
  } else {
    const std::string& s = std::get<std::string>(value);
    rc = sqlite3_bind_text(stmt_, index, s.data(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
  }
  if (rc != SQLITE_OK) {
    throw SqliteError("failed to bind parameter at index " + std::to_string(index));
  }
}

bool SqliteStatement::step() {
  int rc = sqlite3_step(stmt_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  throw SqliteError("SQLite step failed: " + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt_))));
}

void SqliteStatement::reset() {
  sqlite3_reset(stmt_);
  sqlite3_clear_bindings(stmt_);
}

SqliteValue SqliteStatement::column(int index) const {
  switch (sqlite3_column_type(stmt_, index)) {
    case SQLITE_NULL:
      return nullptr;
    case SQLITE_INTEGER:
      return static_cast<std::int64_t>(sqlite3_column_int64(stmt_, index));
    case SQLITE_FLOAT:
      return sqlite3_column_double(stmt_, index);
    default: {
      const unsigned char* text = sqlite3_column_text(stmt_, index);
      int len = sqlite3_column_bytes(stmt_, index);
      return std::string(reinterpret_cast<const char*>(text), len);
    }
  }
}

int SqliteStatement::column_count() const { return sqlite3_column_count(stmt_); }

}  // namespace bazaartalks::storage
