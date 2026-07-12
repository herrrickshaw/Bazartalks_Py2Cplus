#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/storage/sqlite_db.hpp"

using namespace bazaartalks::storage;

TEST_CASE("SqliteDb create table, insert, and query round-trip", "[sqlite]") {
  SqliteDb db(":memory:");
  db.execute("CREATE TABLE fund(ticker TEXT, roe REAL, mktcap INTEGER)");

  auto ins = db.prepare("INSERT INTO fund(ticker, roe, mktcap) VALUES (?, ?, ?)");
  ins.bind(1, std::string("AAPL"));
  ins.bind(2, 34.5);
  ins.bind(3, static_cast<std::int64_t>(3000000000000));
  REQUIRE_FALSE(ins.step());  // INSERT has no result rows -> step() returns false (SQLITE_DONE)

  auto sel = db.prepare("SELECT ticker, roe, mktcap FROM fund WHERE ticker = ?");
  sel.bind(1, std::string("AAPL"));
  REQUIRE(sel.step());
  CHECK(std::get<std::string>(sel.column(0)) == "AAPL");
  CHECK(std::get<double>(sel.column(1)) == 34.5);
  CHECK(std::get<std::int64_t>(sel.column(2)) == 3000000000000);
  CHECK_FALSE(sel.step());  // no more rows
}

TEST_CASE("SqliteStatement::column returns nullptr_t for NULL columns", "[sqlite]") {
  SqliteDb db(":memory:");
  db.execute("CREATE TABLE t(x TEXT)");
  db.execute("INSERT INTO t(x) VALUES (NULL)");

  auto sel = db.prepare("SELECT x FROM t");
  REQUIRE(sel.step());
  CHECK(std::holds_alternative<std::nullptr_t>(sel.column(0)));
}

TEST_CASE("SqliteDb::execute throws SqliteError on malformed SQL", "[sqlite]") {
  SqliteDb db(":memory:");
  CHECK_THROWS_AS(db.execute("NOT VALID SQL AT ALL"), SqliteError);
}
