// Port of serve.py's HTTP routes to Drogon. Wires together the pure,
// already-tested logic (bazaartalks::quant_core::build_query()/
// validate_predicate() for the SQL-injection gate, ::charts::line_chart()
// for the standalone SVG chart) with the already-ported Warehouse
// (Phase 2) for the actual DuckDB reads.
//
// Scope boundary: this file itself (route registration, request/response
// marshalling, argv parsing) is data-plumbing -- untested here by design,
// matching every other phase's "main() is out of scope" boundary; the
// logic it calls into IS tested, in libs/quant_core/query_catalog,
// libs/quant_core/charts, and libs/storage/duckdb_client.
//
// NOT ported (deferred, not silently dropped -- see
// libs/quant_core/query_catalog's header for the full rationale): the
// `/watchlists` CRUD routes, which depend on watchlist_store.py's own
// versioned-CRUD ("vcrud") schema/semantics, a substantial side quest
// orthogonal to this phase's actual gate (the read-only `/filter`
// predicate validator).
#include "bazaartalks/quant_core/charts.hpp"
#include "bazaartalks/quant_core/query_catalog.hpp"
#include "bazaartalks/storage/warehouse.hpp"

#include <drogon/HttpAppFramework.h>
#include <drogon/drogon.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>

using namespace bazaartalks::quant_core;
using bazaartalks::storage::Warehouse;
using bazaartalks::storage::WarehousePaths;
using namespace drogon;

namespace {

// One Warehouse per request, matching serve.py's own `_connect()`/
// `con.close()` per-request pattern (views are rebuilt each session,
// matching the Python original's own comment: "views are per-session").
// `duckdb_path`/`paths` come from argv/env at startup, not hardcoded.
std::string g_duckdb_path = "market.duckdb";
WarehousePaths g_paths;

std::unique_ptr<Warehouse> connect() {
  auto wh = std::make_unique<Warehouse>(g_duckdb_path, g_paths);
  wh->build();
  return wh;
}

// Converts a space-separated DuckDB date/timestamp string
// ("2026-06-26 00:00:00") to pandas' ISO-8601 "T"-separated form
// ("2026-06-26T00:00:00") -- found by diffing /ticker responses against
// the real serve.py during the Phase 9 parallel-run cutover. Detected by
// STRING SHAPE (a space at position 10 of a "YYYY-MM-DD HH:MM:SS"-length
// value) rather than by caller-supplied type, so every DATE/TIME/
// TIMESTAMP[_SEC|_MS|_NS|_TZ] variant is covered uniformly, including
// ones added to a future DuckDB version this code doesn't know about yet.
std::string normalize_datetime_string(std::string s) {
  if (s.size() >= 19 && s[4] == '-' && s[7] == '-' && s[10] == ' ') {
    s[10] = 'T';
  }
  return s;
}

// jsoncpp's own double handling is only half-safe: a NaN silently becomes
// JSON `null` (fine), but +-Infinity becomes the numeric literal
// `1e+9999` -- a real, finite-looking (but wrong) number, not an error and
// not `null` either. Real fundamentals data legitimately produces both
// (a non-dividend stock's div_yield is NaN; a divide-by-zero P/E is
// +-Infinity), confirmed by running the real serve.py's /filter endpoint
// against real production data (BRK-B's div_yield). Normalize both to
// `null` explicitly rather than trusting jsoncpp's inconsistent default.
Json::Value json_double(double x) { return std::isfinite(x) ? Json::Value(x) : Json::Value::null; }

// Converts one column's value to the JSON type that actually matches its
// SQL type, rather than collapsing every non-string column into a
// double. This matters beyond precision (see GetValue<double>()'s own
// history below): an INTEGER/BIGINT column (e.g. dvm_global.py's
// `above_200dma INT` boolean-flag columns, or a `count(*)` result)
// serialized as a JSON *integer* is both smaller on the wire and matches
// what pandas'/Python's own json encoder emits for the equivalent int64
// column (`1`, not `1.0`) -- a real, if easy to miss, cross-language
// wire-format difference the Phase 9 cutover's HTTP-level diffing was
// specifically built to catch.
template <typename Row>
Json::Value column_to_json(Row& row, std::size_t c, const duckdb::LogicalType& type) {
  if (row.IsNull(c)) return Json::Value::null;
  using Id = duckdb::LogicalTypeId;
  switch (type.id()) {
    case Id::BOOLEAN:
      return row.template GetValue<bool>(c);
    case Id::TINYINT:
    case Id::SMALLINT:
    case Id::INTEGER:
    case Id::BIGINT:
    case Id::UTINYINT:
    case Id::USMALLINT:
    case Id::UINTEGER:
    case Id::UBIGINT:
      return static_cast<Json::Int64>(row.template GetValue<int64_t>(c));
    case Id::HUGEINT:
    case Id::UHUGEINT:
      // Wider than int64_t in the general case; this schema never
      // actually produces a value that large (largest real column here is
      // mktcap, comfortably inside int64_t), so falling back to DOUBLE
      // (DuckDB's own overflow-safe read path) rather than risking
      // GetValue<int64_t>() truncating is the conservative choice, not a
      // claim this is lossless for arbitrary HUGEINT data.
      return json_double(row.template GetValue<double>(c));
    case Id::FLOAT:
    case Id::DOUBLE:
    case Id::DECIMAL:
      // GetValue<double>() reads the raw column value directly. Do NOT
      // route numeric columns through GetValue<std::string>() then
      // std::stod() -- DuckDB's string conversion for a DOUBLE uses a
      // human-readable, reduced-precision representation (verified
      // against real OHLC data during the Phase 9 parallel-run cutover:
      // an actual Close price of 283.779999 round-tripped through that
      // string conversion came back as 283.78, silently discarding real
      // precision every numeric route serves). json_double() (not a bare
      // Json::Value(double)) additionally normalizes NaN/+-Infinity to
      // `null`, matching serve.py's own warehouse.json_safe() fix for the
      // same real-world NaN/Infinity data.
      return json_double(row.template GetValue<double>(c));
    case Id::DATE:
    case Id::TIME:
    case Id::TIME_TZ:
    case Id::TIME_NS:
    case Id::TIMESTAMP:
    case Id::TIMESTAMP_SEC:
    case Id::TIMESTAMP_MS:
    case Id::TIMESTAMP_NS:
    case Id::TIMESTAMP_TZ:
      return normalize_datetime_string(row.template GetValue<std::string>(c));
    default:
      // VARCHAR and anything else not enumerated above (BLOB, ENUM,
      // UUID, ...) -- read as text, matching how DuckDB itself renders
      // them when no more specific handling applies.
      return row.template GetValue<std::string>(c);
  }
}

Json::Value row_to_json(duckdb::MaterializedQueryResult& result) {
  Json::Value rows(Json::arrayValue);
  auto& names = result.names;
  auto& types = result.types;
  for (auto& row : result) {
    Json::Value obj;
    for (std::size_t c = 0; c < names.size(); ++c) {
      obj[names[c]] = column_to_json(row, c, types[c]);
    }
    rows.append(obj);
  }
  return rows;
}

HttpResponsePtr json_error(HttpStatusCode code, const std::string& message) {
  Json::Value j;
  j["error"] = message;
  auto resp = HttpResponse::newHttpJsonResponse(j);
  resp->setStatusCode(code);
  return resp;
}

// Strict allow-list for a raw ticker/market path or query param used to
// build SQL directly (not going through validate_predicate, which is for
// the `/filter` predicate specifically) -- alphanumeric, dot, underscore,
// hyphen only. Matches the practical shape of every real ticker/market
// code in this platform (e.g. "7203.T", "US", "RELIANCE") while refusing
// anything that could break out of a quoted SQL string literal.
bool is_safe_token(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--duckdb" && i + 1 < argc) {
      g_duckdb_path = argv[++i];
    } else if (arg == "--ohlc-dir" && i + 1 < argc) {
      g_paths.ohlc_parquet_dirs.push_back(argv[++i]);
    } else if (arg == "--fundamentals-db" && i + 1 < argc) {
      g_paths.fundamentals_cache_db = argv[++i];
    } else if (arg == "--dvm-global-db" && i + 1 < argc) {
      g_paths.dvm_global_db = argv[++i];
    } else if (arg == "--dvm-composite-db" && i + 1 < argc) {
      g_paths.dvm_composite_db = argv[++i];
    }
  }

  app().registerHandler(
      "/health",
      [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value j;
        j["status"] = "ok";
        callback(HttpResponse::newHttpJsonResponse(j));
      },
      {Get});

  app().registerHandler(
      "/markets",
      [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
        auto wh = connect();
        auto result = wh->query(build_query("markets"));
        callback(HttpResponse::newHttpJsonResponse(row_to_json(*result)));
      },
      {Get});

  app().registerHandler(
      "/ggg",
      [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
        int limit = 25;
        if (auto l = req->getOptionalParameter<int>("limit")) limit = *l;
        std::string q = build_query("ggg", limit);
        auto market = req->getOptionalParameter<std::string>("market");
        if (market && !market->empty()) {
          if (!is_safe_token(*market)) {
            callback(json_error(k400BadRequest, "invalid market"));
            return;
          }
          std::string needle = "WHERE code='GGG'";
          auto pos = q.find(needle);
          if (pos != std::string::npos) {
            q.replace(pos, needle.size(), needle + " AND market='" + *market + "'");
          }
        }
        auto wh = connect();
        auto result = wh->query(q);
        callback(HttpResponse::newHttpJsonResponse(row_to_json(*result)));
      },
      {Get});

  app().registerHandler(
      "/screen/{name}",
      [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback,
         std::string name) {
        int limit = 25;
        if (auto l = req->getOptionalParameter<int>("limit")) limit = *l;
        std::string q;
        try {
          q = build_query(name, limit);
        } catch (const QueryCatalogError& e) {
          callback(json_error(k404NotFound, e.what()));
          return;
        }
        auto wh = connect();
        auto result = wh->query(q);
        callback(HttpResponse::newHttpJsonResponse(row_to_json(*result)));
      },
      {Get});

  app().registerHandler(
      "/filter",
      [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
        auto predicate = req->getOptionalParameter<std::string>("predicate");
        if (!predicate) {
          callback(json_error(k400BadRequest, "missing predicate"));
          return;
        }
        int limit = 30;
        if (auto l = req->getOptionalParameter<int>("limit")) limit = *l;
        std::string pred;
        try {
          pred = validate_predicate(*predicate);
        } catch (const QueryCatalogError& e) {
          callback(json_error(k400BadRequest, e.what()));
          return;
        }
        // dvm_composite carries its own roe/de/pe (denormalized at DVM-
        // composite build time), which collide with fundamentals'
        // identically-named columns once joined -- an unqualified
        // predicate like "roe>15" (validate_predicate's allow-list has no
        // way to accept a qualified "c.roe" either) used to raise DuckDB's
        // "Ambiguous reference to column name" error, confirmed against
        // the real (unmodified) serve.py against real production data.
        // Fixed identically on both sides: join only fundamentals' columns
        // that DON'T already exist on dvm_composite, and read roe/de/pe
        // from dvm_composite's own copy (also what's actually displayed
        // for those fields in the same row, so a predicate and its
        // displayed values are now guaranteed consistent).
        std::string q =
            "SELECT c.market, c.ticker, c.D, c.V, c.M, c.composite, c.code, "
            "c.roe, c.de, c.pe, "
            "f.pb, f.roa, f.rev_growth, f.earn_growth, f.op_margin, f.div_yield, f.mktcap "
            "FROM dvm_composite c "
            "LEFT JOIN (SELECT ticker, pb, roa, rev_growth, earn_growth, op_margin, "
            "                  div_yield, mktcap FROM fundamentals) f "
            "  ON c.ticker=f.ticker "
            "WHERE " +
            pred + " ORDER BY c.composite DESC LIMIT " + std::to_string(limit);
        auto wh = connect();
        auto result = wh->query(q);
        callback(HttpResponse::newHttpJsonResponse(row_to_json(*result)));
      },
      {Get});

  app().registerHandler(
      "/ticker/{symbol}",
      [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback,
         std::string symbol) {
        auto market = req->getOptionalParameter<std::string>("market");
        if (!market) {
          callback(json_error(k400BadRequest, "missing market"));
          return;
        }
        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
        std::string mkt = *market;
        std::transform(mkt.begin(), mkt.end(), mkt.begin(), ::toupper);
        if (!is_safe_token(symbol) || !is_safe_token(mkt)) {
          callback(json_error(k400BadRequest, "invalid symbol/market"));
          return;
        }
        auto wh = connect();
        // Port of warehouse.py's ticker_detail(): OHLC history plus
        // whichever of fundamentals/dvm_global/dvm_composite are
        // currently built, silently omitted (null) rather than erroring
        // if a view is absent -- matching the "skip what's absent"
        // convention `available = {SHOW TABLES}` enforces there.
        auto available_vec = wh->tables();
        std::set<std::string> available(available_vec.begin(), available_vec.end());

        Json::Value j;
        j["ticker"] = symbol;
        j["market"] = mkt;

        if (available.count("ohlc")) {
          auto ohlc = wh->query("SELECT Date, Open, High, Low, Close, Volume FROM ohlc "
                                "WHERE ticker='" +
                                symbol + "' AND market='" + mkt +
                                "' ORDER BY Date DESC LIMIT 60");
          j["ohlc"] = row_to_json(*ohlc);
        } else {
          j["ohlc"] = Json::Value(Json::arrayValue);
        }

        auto join_view = [&](const std::string& view, const std::string& key) {
          if (!available.count(view)) {
            j[key] = Json::Value::null;
            return;
          }
          auto result = wh->query("SELECT * FROM " + view + " WHERE ticker='" + symbol +
                                  "' AND market='" + mkt + "' LIMIT 1");
          auto rows = row_to_json(*result);
          j[key] = rows.empty() ? Json::Value::null : rows[0];
        };
        join_view("fundamentals", "fundamentals");
        join_view("dvm_global", "dvm_technical");
        join_view("dvm_composite", "dvm_composite");

        bool has_any = !j["ohlc"].empty() || !j["fundamentals"].isNull() ||
                      !j["dvm_technical"].isNull() || !j["dvm_composite"].isNull();
        if (!has_any) {
          callback(json_error(k404NotFound, "no data for " + symbol + "/" + mkt));
          return;
        }
        callback(HttpResponse::newHttpJsonResponse(j));
      },
      {Get});

  app().registerHandler(
      "/chart/ticker/{symbol}",
      [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback,
         std::string symbol) {
        auto market = req->getOptionalParameter<std::string>("market");
        int bars = 60;
        if (auto b = req->getOptionalParameter<int>("bars")) bars = *b;
        if (!market) {
          callback(json_error(k400BadRequest, "missing market"));
          return;
        }
        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
        std::string mkt = *market;
        std::transform(mkt.begin(), mkt.end(), mkt.begin(), ::toupper);
        if (!is_safe_token(symbol) || !is_safe_token(mkt)) {
          callback(json_error(k400BadRequest, "invalid symbol/market"));
          return;
        }
        auto wh = connect();
        auto result = wh->query("SELECT Date, Close FROM ohlc WHERE ticker='" + symbol +
                                "' AND market='" + mkt + "' ORDER BY Date DESC LIMIT " +
                                std::to_string(bars));
        std::vector<std::string> dates;
        std::vector<double> closes;
        for (auto& row : *result) {
          // Python: `str(row["Date"])[:10]` -- just the date, no
          // time-of-day component, for the chart's x-axis tick labels.
          // Found missing here by inspecting the actual SVG content
          // during a Phase 9 cutover follow-up (the earlier comparison
          // only checked HTTP status/content-type for this route, not
          // the rendered label text).
          dates.push_back(row.GetValue<std::string>(0).substr(0, 10));
          closes.push_back(row.GetValue<double>(1));
        }
        if (closes.empty()) {
          callback(json_error(k404NotFound, "no OHLC data for " + symbol + "/" + mkt));
          return;
        }
        std::reverse(dates.begin(), dates.end());
        std::reverse(closes.begin(), closes.end());
        std::string svg = line_chart(dates, closes, 600, 160,
                                     symbol + " close, last " +
                                         std::to_string(closes.size()) + " bars");
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeString("image/svg+xml");
        resp->setBody(svg);
        callback(resp);
      },
      {Get});

  std::cout << "[serve_api] duckdb=" << g_duckdb_path << std::endl;
  app().addListener("127.0.0.1", 8000);
  app().run();
  return 0;
}
