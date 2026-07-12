#include "bazaartalks/storage/databento_client.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>

#include "bazaartalks/storage/sqlite_db.hpp"
#include "databento/constants.hpp"
#include "databento/enums.hpp"
#include "databento/historical.hpp"

namespace bazaartalks::storage {

namespace db = databento;

namespace {

constexpr const char* kCreateOhlcv1M =
    "CREATE TABLE IF NOT EXISTS ohlcv_1m ("
    "  symbol TEXT NOT NULL,"
    "  ts_event_ns INTEGER NOT NULL,"
    "  date TEXT NOT NULL,"
    "  open REAL, high REAL, low REAL, close REAL, volume INTEGER,"
    "  PRIMARY KEY (symbol, ts_event_ns)"
    ")";
constexpr const char* kIndexOhlcv1M =
    "CREATE INDEX IF NOT EXISTS ix_ohlcv_1m_symbol_date ON ohlcv_1m(symbol, date)";

constexpr const char* kCreateTbbo =
    "CREATE TABLE IF NOT EXISTS tbbo ("
    "  symbol TEXT NOT NULL,"
    "  ts_event_ns INTEGER NOT NULL,"
    "  date TEXT NOT NULL,"
    "  price REAL, size INTEGER,"
    "  bid_px REAL, ask_px REAL, bid_sz INTEGER, ask_sz INTEGER,"
    "  PRIMARY KEY (symbol, ts_event_ns)"
    ")";
constexpr const char* kIndexTbbo =
    "CREATE INDEX IF NOT EXISTS ix_tbbo_symbol_date ON tbbo(symbol, date)";

constexpr const char* kCreateSpendLedger =
    "CREATE TABLE IF NOT EXISTS spend_ledger ("
    "  logged_at TEXT NOT NULL,"
    "  dataset TEXT NOT NULL,"
    "  symbol TEXT NOT NULL,"
    "  schema TEXT NOT NULL,"
    "  start_date TEXT NOT NULL,"
    "  end_date TEXT NOT NULL,"
    "  estimated_cost_usd REAL NOT NULL"
    ")";

std::string table_for(DatabentoSchema schema) {
  return schema == DatabentoSchema::Ohlcv1M ? "ohlcv_1m" : "tbbo";
}

db::Schema vendor_schema_for(DatabentoSchema schema) {
  return schema == DatabentoSchema::Ohlcv1M ? db::Schema::Ohlcv1M : db::Schema::Tbbo;
}

double from_fixed_price(std::int64_t fixed) {
  if (fixed == db::kUndefPrice) return 0.0;
  return static_cast<double>(fixed) / static_cast<double>(db::kFixedPriceScale);
}

// UTC calendar date ("YYYY-MM-DD") for a UnixNanos timestamp -- matches the
// gmtime_r/put_time style already used by the Kafka consumers' main.cpp for
// wall-clock formatting, rather than pulling in a second date-handling
// convention just for this module.
std::string date_from_ts_event_ns(std::int64_t ts_event_ns) {
  std::time_t seconds = static_cast<std::time_t>(ts_event_ns / 1'000'000'000LL);
  std::tm utc_tm{};
  gmtime_r(&seconds, &utc_tm);
  char buf[11];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", &utc_tm);
  return std::string(buf);
}

std::string iso_now() {
  std::time_t now = std::time(nullptr);
  std::tm utc_tm{};
  gmtime_r(&now, &utc_tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
  return std::string(buf);
}

// A day after `date` ("YYYY-MM-DD"), for turning an inclusive max-covered-date
// into the exclusive start of the next delta request.
std::string next_day(const std::string& date) {
  std::tm tm{};
  strptime(date.c_str(), "%Y-%m-%d", &tm);
  std::time_t t = timegm(&tm);
  t += 24 * 60 * 60;
  std::tm next_tm{};
  gmtime_r(&t, &next_tm);
  char buf[11];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", &next_tm);
  return std::string(buf);
}

}  // namespace

CostGateDecision evaluate_cost_gate(double estimated_cost_usd, double threshold_usd,
                                     std::optional<double> confirmed_cost_usd) {
  if (estimated_cost_usd <= threshold_usd) {
    return {true, "estimated cost $" + std::to_string(estimated_cost_usd) +
                      " is within the $" + std::to_string(threshold_usd) + " threshold"};
  }
  if (confirmed_cost_usd.has_value()) {
    double relative_diff = std::abs(*confirmed_cost_usd - estimated_cost_usd) /
                           std::max(estimated_cost_usd, 0.01);
    if (relative_diff <= 0.01) {
      return {true, "override confirmed, matches current estimate ($" +
                        std::to_string(estimated_cost_usd) + ")"};
    }
    return {false, "override amount $" + std::to_string(*confirmed_cost_usd) +
                       " does not match the current estimate of $" +
                       std::to_string(estimated_cost_usd) +
                       " -- re-run estimate_cost() and pass the real figure back"};
  }
  return {false, "estimated cost $" + std::to_string(estimated_cost_usd) +
                     " exceeds the $" + std::to_string(threshold_usd) +
                     " threshold; pass confirmed_cost_usd=" + std::to_string(estimated_cost_usd) +
                     " to proceed"};
}

std::string to_string(DatabentoSchema schema) { return table_for(schema); }

struct DatabentoClient::Impl {
  SqliteDb db;
  double cost_threshold_usd;

  explicit Impl(const std::string& path, double threshold)
      : db(path), cost_threshold_usd(threshold) {
    db.execute(kCreateOhlcv1M);
    db.execute(kIndexOhlcv1M);
    db.execute(kCreateTbbo);
    db.execute(kIndexTbbo);
    db.execute(kCreateSpendLedger);
  }

  void log_spend(const std::string& dataset, const std::string& symbol,
                 const std::string& schema_name, const std::string& start_date,
                 const std::string& end_date, double estimated_cost_usd) {
    auto stmt = db.prepare(
        "INSERT INTO spend_ledger(logged_at, dataset, symbol, schema, start_date, end_date, "
        "estimated_cost_usd) VALUES (?, ?, ?, ?, ?, ?, ?)");
    stmt.bind(1, iso_now());
    stmt.bind(2, dataset);
    stmt.bind(3, symbol);
    stmt.bind(4, schema_name);
    stmt.bind(5, start_date);
    stmt.bind(6, end_date);
    stmt.bind(7, estimated_cost_usd);
    stmt.step();
  }
};

DatabentoClient::DatabentoClient(std::string bars_db_path, double cost_threshold_usd)
    : impl_(std::make_unique<Impl>(bars_db_path, cost_threshold_usd)) {}

DatabentoClient::~DatabentoClient() = default;

std::optional<DatabentoCoverage> DatabentoClient::coverage(const std::string& symbol,
                                                            DatabentoSchema schema) {
  auto stmt = impl_->db.prepare("SELECT min(date), max(date), count(*) FROM " +
                                table_for(schema) + " WHERE symbol = ?");
  stmt.bind(1, symbol);
  if (!stmt.step()) return std::nullopt;

  auto min_v = stmt.column(0);
  if (std::holds_alternative<std::nullptr_t>(min_v)) return std::nullopt;

  DatabentoCoverage cov;
  cov.min_date = std::get<std::string>(min_v);
  cov.max_date = std::get<std::string>(stmt.column(1));
  cov.n = std::get<std::int64_t>(stmt.column(2));
  return cov;
}

double DatabentoClient::estimate_cost(const std::string& dataset, const std::string& symbol,
                                      DatabentoSchema schema, const std::string& start_date,
                                      const std::string& end_date) {
  auto client = db::Historical::Builder().SetKeyFromEnv().Build();
  return client.MetadataGetCost(dataset, db::DateRange{start_date, end_date}, {symbol},
                                vendor_schema_for(schema));
}

std::size_t DatabentoClient::fetch_and_store(const std::string& dataset,
                                             const std::string& symbol, DatabentoSchema schema,
                                             const std::string& start_date,
                                             const std::string& end_date,
                                             std::optional<double> confirmed_cost_usd) {
  std::string delta_start = start_date;
  if (auto cov = coverage(symbol, schema)) {
    if (cov->max_date >= end_date) {
      return 0;  // fully covered already -- no network call at all, no cost.
    }
    if (cov->max_date >= delta_start) {
      delta_start = next_day(cov->max_date);
    }
  }

  double estimated = estimate_cost(dataset, symbol, schema, delta_start, end_date);
  auto decision = evaluate_cost_gate(estimated, impl_->cost_threshold_usd, confirmed_cost_usd);
  if (!decision.allowed) {
    throw DatabentoCostExceeded(decision.reason);
  }
  impl_->log_spend(dataset, symbol, table_for(schema), delta_start, end_date, estimated);

  auto client = db::Historical::Builder().SetKeyFromEnv().Build();
  auto store = client.TimeseriesGetRange(dataset, db::DateRange{delta_start, end_date}, {symbol},
                                        vendor_schema_for(schema));

  std::size_t written = 0;
  if (schema == DatabentoSchema::Ohlcv1M) {
    auto stmt = impl_->db.prepare(
        "INSERT OR IGNORE INTO ohlcv_1m(symbol, ts_event_ns, date, open, high, low, close, "
        "volume) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    while (const auto* record = store.NextRecord()) {
      const auto& bar = record->Get<db::OhlcvMsg>();
      auto ts_ns = static_cast<std::int64_t>(bar.hd.ts_event.time_since_epoch().count());
      stmt.bind(1, symbol);
      stmt.bind(2, ts_ns);
      stmt.bind(3, date_from_ts_event_ns(ts_ns));
      stmt.bind(4, from_fixed_price(bar.open));
      stmt.bind(5, from_fixed_price(bar.high));
      stmt.bind(6, from_fixed_price(bar.low));
      stmt.bind(7, from_fixed_price(bar.close));
      stmt.bind(8, static_cast<std::int64_t>(bar.volume));
      stmt.step();
      stmt.reset();
      ++written;
    }
  } else {
    auto stmt = impl_->db.prepare(
        "INSERT OR IGNORE INTO tbbo(symbol, ts_event_ns, date, price, size, bid_px, ask_px, "
        "bid_sz, ask_sz) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    while (const auto* record = store.NextRecord()) {
      const auto& quote = record->Get<db::Mbp1Msg>();
      auto ts_ns = static_cast<std::int64_t>(quote.hd.ts_event.time_since_epoch().count());
      const auto& level = quote.levels[0];
      stmt.bind(1, symbol);
      stmt.bind(2, ts_ns);
      stmt.bind(3, date_from_ts_event_ns(ts_ns));
      stmt.bind(4, from_fixed_price(quote.price));
      stmt.bind(5, static_cast<std::int64_t>(quote.size));
      stmt.bind(6, from_fixed_price(level.bid_px));
      stmt.bind(7, from_fixed_price(level.ask_px));
      stmt.bind(8, static_cast<std::int64_t>(level.bid_sz));
      stmt.bind(9, static_cast<std::int64_t>(level.ask_sz));
      stmt.step();
      stmt.reset();
      ++written;
    }
  }
  return written;
}

}  // namespace bazaartalks::storage
