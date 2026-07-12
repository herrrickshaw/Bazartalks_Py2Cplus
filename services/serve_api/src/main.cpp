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
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>

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

Json::Value row_to_json(duckdb::MaterializedQueryResult& result) {
  Json::Value rows(Json::arrayValue);
  auto& names = result.names;
  auto& types = result.types;
  for (auto& row : result) {
    Json::Value obj;
    for (std::size_t c = 0; c < names.size(); ++c) {
      auto val = row.GetValue<std::string>(c);
      if (row.IsNull(c)) {
        obj[names[c]] = Json::Value::null;
      } else if (types[c].IsNumeric()) {
        try {
          obj[names[c]] = std::stod(val);
        } catch (...) {
          obj[names[c]] = val;
        }
      } else {
        obj[names[c]] = val;
      }
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
        std::string q =
            "SELECT c.market, c.ticker, c.D, c.V, c.M, c.composite, c.code, "
            "f.roe, f.de, f.pe FROM dvm_composite c "
            "LEFT JOIN fundamentals f ON c.ticker=f.ticker "
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
        auto ohlc = wh->query("SELECT Date, Open, High, Low, Close, Volume FROM ohlc "
                              "WHERE ticker='" +
                              symbol + "' AND market='" + mkt +
                              "' ORDER BY Date DESC LIMIT 60");
        Json::Value j;
        j["ticker"] = symbol;
        j["market"] = mkt;
        j["ohlc"] = row_to_json(*ohlc);
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
          dates.push_back(row.GetValue<std::string>(0));
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
