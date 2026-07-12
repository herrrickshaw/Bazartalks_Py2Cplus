#include "bazaartalks/storage/market_store.hpp"

#include <cassandra.h>

#include <cstring>
#include <memory>
#include <sstream>

#include "bazaartalks/streaming/kafka_producer.hpp"

namespace bazaartalks::storage {

namespace {

// Every table reference is fully-qualified (market.ohlc_bars) instead of
// relying on a session-level `USE market` -- avoids an extra round trip and
// keeps every query self-contained regardless of session state.
constexpr const char* kKeyspaceDdl =
    "CREATE KEYSPACE IF NOT EXISTS market WITH replication="
    "{'class':'SimpleStrategy','replication_factor':1}";
constexpr const char* kTableDdl =
    "CREATE TABLE IF NOT EXISTS market.ohlc_bars("
    "ticker text, d date, o double, h double, l double, c double, v double, "
    "PRIMARY KEY (ticker, d))";
constexpr const char* kInsertCql =
    "INSERT INTO market.ohlc_bars(ticker,d,o,h,l,c,v) VALUES (?,?,?,?,?,?,?)";
constexpr const char* kSelectCql = "SELECT d,o,h,l,c,v FROM market.ohlc_bars WHERE ticker=?";
constexpr const char* kCoverageCql =
    "SELECT MIN(d) AS mn, MAX(d) AS mx, COUNT(*) AS n FROM market.ohlc_bars WHERE ticker=?";

// RAII wrappers -- the Cassandra C API has no C++ ownership types of its
// own; every *_free() must be paired with its *_new()/allocation call.
struct FutureDeleter {
  void operator()(CassFuture* f) const {
    if (f) cass_future_free(f);
  }
};
using FuturePtr = std::unique_ptr<CassFuture, FutureDeleter>;

struct StatementDeleter {
  void operator()(CassStatement* s) const {
    if (s) cass_statement_free(s);
  }
};
using StatementPtr = std::unique_ptr<CassStatement, StatementDeleter>;

struct ResultDeleter {
  void operator()(const CassResult* r) const {
    if (r) cass_result_free(r);
  }
};
using ResultPtr = std::unique_ptr<const CassResult, ResultDeleter>;

struct IteratorDeleter {
  void operator()(CassIterator* i) const {
    if (i) cass_iterator_free(i);
  }
};
using IteratorPtr = std::unique_ptr<CassIterator, IteratorDeleter>;

struct PreparedDeleter {
  void operator()(const CassPrepared* p) const {
    if (p) cass_prepared_free(p);
  }
};
using PreparedPtr = std::unique_ptr<const CassPrepared, PreparedDeleter>;

std::string future_error_message(CassFuture* future) {
  const char* msg = nullptr;
  size_t len = 0;
  cass_future_error_message(future, &msg, &len);
  return std::string(msg, len);
}

// Blocks until `future` resolves; throws `Err` (constructed from `context`
// + the driver's error message) if it resolved to an error.
template <typename Err>
void check_future(CassFuture* future, const std::string& context) {
  cass_future_wait(future);
  CassError rc = cass_future_error_code(future);
  if (rc != CASS_OK) {
    throw Err(context + ": " + future_error_message(future));
  }
}

date::year_month_day cass_date_to_ymd(cass_uint32_t d) {
  cass_int64_t epoch_secs = cass_date_time_to_epoch(d, 0);
  return date::year_month_day{date::sys_days{date::days{epoch_secs / 86400}}};
}

cass_uint32_t ymd_to_cass_date(date::year_month_day ymd) {
  auto epoch_secs =
      std::chrono::duration_cast<std::chrono::seconds>(date::sys_days{ymd}.time_since_epoch())
          .count();
  return cass_date_from_epoch(epoch_secs);
}

}  // namespace

struct MarketStore::Impl {
  CassCluster* cluster = nullptr;
  CassSession* session = nullptr;
  PreparedPtr prep_insert;
  PreparedPtr prep_select;
  PreparedPtr prep_coverage;
  std::unique_ptr<streaming::KafkaProducer> kafka;

  ~Impl() {
    if (session) {
      FuturePtr close_future(cass_session_close(session));
      cass_future_wait(close_future.get());
      cass_session_free(session);
    }
    if (cluster) cass_cluster_free(cluster);
  }
};

MarketStore::MarketStore(const std::string& contact_points, int port,
                          std::chrono::milliseconds connect_timeout,
                          const std::string& kafka_bootstrap)
    : impl_(new Impl) {
  impl_->cluster = cass_cluster_new();
  cass_cluster_set_contact_points(impl_->cluster, contact_points.c_str());
  cass_cluster_set_port(impl_->cluster, port);
  cass_cluster_set_connect_timeout(impl_->cluster, static_cast<unsigned>(connect_timeout.count()));

  impl_->session = cass_session_new();
  FuturePtr connect_future(cass_session_connect(impl_->session, impl_->cluster));
  cass_future_wait(connect_future.get());
  if (cass_future_error_code(connect_future.get()) != CASS_OK) {
    std::string msg = future_error_message(connect_future.get());
    cass_session_free(impl_->session);
    impl_->session = nullptr;
    throw MarketStoreUnavailable(
        "Cassandra is required by market_store.py but is unreachable: " + msg);
  }

  // Schema setup (CREATE KEYSPACE / CREATE TABLE IF NOT EXISTS) -- mirrors
  // _connect()'s idempotent setup, run every time like the Python original.
  for (const char* ddl : {kKeyspaceDdl, kTableDdl}) {
    StatementPtr stmt(cass_statement_new(ddl, 0));
    FuturePtr future(cass_session_execute(impl_->session, stmt.get()));
    check_future<MarketStoreUnavailable>(future.get(), "failed to initialize Cassandra schema");
  }

  auto prepare = [&](const char* cql) {
    FuturePtr future(cass_session_prepare(impl_->session, cql));
    check_future<MarketStoreUnavailable>(future.get(), std::string("failed to prepare: ") + cql);
    return PreparedPtr(cass_future_get_prepared(future.get()));
  };
  impl_->prep_insert = prepare(kInsertCql);
  impl_->prep_select = prepare(kSelectCql);
  impl_->prep_coverage = prepare(kCoverageCql);

  impl_->kafka = std::make_unique<streaming::KafkaProducer>(kafka_bootstrap);
}

MarketStore::~MarketStore() = default;

std::optional<Coverage> MarketStore::coverage(const std::string& ticker) {
  StatementPtr stmt(cass_prepared_bind(impl_->prep_coverage.get()));
  cass_statement_bind_string(stmt.get(), 0, ticker.c_str());

  FuturePtr future(cass_session_execute(impl_->session, stmt.get()));
  check_future<MarketStoreUnavailable>(future.get(), "coverage() query failed");

  ResultPtr result(cass_future_get_result(future.get()));
  const CassRow* row = cass_result_first_row(result.get());
  if (!row) return std::nullopt;

  const CassValue* mn_val = cass_row_get_column(row, 0);
  const CassValue* mx_val = cass_row_get_column(row, 1);
  const CassValue* n_val = cass_row_get_column(row, 2);
  if (cass_value_is_null(mn_val) || cass_value_is_null(n_val)) return std::nullopt;

  cass_uint32_t mn_raw = 0, mx_raw = 0;
  cass_int64_t n_raw = 0;
  cass_value_get_uint32(mn_val, &mn_raw);
  cass_value_get_uint32(mx_val, &mx_raw);
  cass_value_get_int64(n_val, &n_raw);
  if (n_raw == 0) return std::nullopt;

  return Coverage{cass_date_to_ymd(mn_raw), cass_date_to_ymd(mx_raw), n_raw};
}

void MarketStore::put_ohlc(const std::string& ticker, const std::vector<OhlcBar>& bars) {
  if (bars.empty()) return;

  std::vector<FuturePtr> futures;
  futures.reserve(bars.size());
  for (const auto& bar : bars) {
    StatementPtr stmt(cass_prepared_bind(impl_->prep_insert.get()));
    cass_statement_bind_string(stmt.get(), 0, ticker.c_str());
    cass_statement_bind_uint32(stmt.get(), 1, ymd_to_cass_date(bar.d));
    cass_statement_bind_double(stmt.get(), 2, bar.o);
    cass_statement_bind_double(stmt.get(), 3, bar.h);
    cass_statement_bind_double(stmt.get(), 4, bar.l);
    cass_statement_bind_double(stmt.get(), 5, bar.c);
    cass_statement_bind_double(stmt.get(), 6, bar.v);
    futures.emplace_back(cass_session_execute(impl_->session, stmt.get()));
  }
  for (auto& f : futures) {
    check_future<std::runtime_error>(f.get(), "Cassandra write failed for ticker '" + ticker + "'");
  }

  // Mandatory CDC publish -- not caught here, propagates
  // streaming::CdcPublishError to the caller on any failure, matching
  // _emit_cdc()'s "raise, don't swallow" contract.
  date::year_month_day latest = bars.front().d;
  for (const auto& bar : bars)
    if (date::sys_days{bar.d} > date::sys_days{latest}) latest = bar.d;

  std::ostringstream json;
  json << R"({"op":"upsert","table":"ohlc_bars","ticker":")" << ticker << R"(","n_bars":)"
       << bars.size() << R"(,"latest":")" << date::format("%F", latest) << R"("})";
  impl_->kafka->publish_and_flush("ohlc.cdc", ticker, json.str());
}

std::vector<OhlcBar> MarketStore::get_ohlc(const std::string& ticker) {
  StatementPtr stmt(cass_prepared_bind(impl_->prep_select.get()));
  cass_statement_bind_string(stmt.get(), 0, ticker.c_str());

  FuturePtr future(cass_session_execute(impl_->session, stmt.get()));
  check_future<MarketStoreUnavailable>(future.get(), "get_ohlc() query failed");

  ResultPtr result(cass_future_get_result(future.get()));
  std::vector<OhlcBar> out;
  IteratorPtr it(cass_iterator_from_result(result.get()));
  while (cass_iterator_next(it.get())) {
    const CassRow* row = cass_iterator_get_row(it.get());
    OhlcBar bar;
    cass_uint32_t d_raw = 0;
    cass_value_get_uint32(cass_row_get_column(row, 0), &d_raw);
    bar.d = cass_date_to_ymd(d_raw);
    cass_value_get_double(cass_row_get_column(row, 1), &bar.o);
    cass_value_get_double(cass_row_get_column(row, 2), &bar.h);
    cass_value_get_double(cass_row_get_column(row, 3), &bar.l);
    cass_value_get_double(cass_row_get_column(row, 4), &bar.c);
    cass_value_get_double(cass_row_get_column(row, 5), &bar.v);
    out.push_back(bar);
  }
  // Cassandra returns rows in clustering-key (date) ascending order for a
  // single partition already, matching Python's df.sort_index().
  return out;
}

}  // namespace bazaartalks::storage
