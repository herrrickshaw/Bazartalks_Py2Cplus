// Thin CLI over bt_databento_client -- data-plumbing (argv parsing, exit
// codes, error-message formatting), not tested here, matching every other
// service's main()-is-plumbing boundary. The actual logic (cost gate,
// coverage-check, DBN decode) lives in libs/storage/databento_client and is
// unit/integration-tested there.
//
// Usage:
//   bt_databento_ingest --dataset DBEQ.BASIC --symbol SPY --schema ohlcv1m \
//     --start 2026-07-01 --end 2026-07-02 --out-db databento_bars.db \
//     [--cost-threshold 1.0] [--confirm-cost <amount>]
//
// Requires DATABENTO_API_KEY in the environment (databento-cpp itself
// throws a clear error if it's unset -- not re-checked here).
#include <iostream>
#include <optional>
#include <string>

#include "bazaartalks/storage/databento_client.hpp"

using bazaartalks::storage::DatabentoClient;
using bazaartalks::storage::DatabentoCostExceeded;
using bazaartalks::storage::DatabentoSchema;

namespace {

void print_usage() {
  std::cout << "usage: bt_databento_ingest --dataset DATASET --symbol SYMBOL "
               "--schema ohlcv1m|tbbo --start YYYY-MM-DD --end YYYY-MM-DD "
               "--out-db PATH [--cost-threshold USD] [--confirm-cost USD]"
            << std::endl;
}

std::optional<DatabentoSchema> parse_schema(const std::string& s) {
  if (s == "ohlcv1m") return DatabentoSchema::Ohlcv1M;
  if (s == "tbbo") return DatabentoSchema::Tbbo;
  return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
  std::string dataset, symbol, schema_arg, start, end, out_db;
  double cost_threshold = 1.0;
  std::optional<double> confirm_cost;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
    if (arg == "--dataset") {
      dataset = next();
    } else if (arg == "--symbol") {
      symbol = next();
    } else if (arg == "--schema") {
      schema_arg = next();
    } else if (arg == "--start") {
      start = next();
    } else if (arg == "--end") {
      end = next();
    } else if (arg == "--out-db") {
      out_db = next();
    } else if (arg == "--cost-threshold") {
      cost_threshold = std::stod(next());
    } else if (arg == "--confirm-cost") {
      confirm_cost = std::stod(next());
    } else if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    }
  }

  if (dataset.empty() || symbol.empty() || schema_arg.empty() || start.empty() || end.empty() ||
      out_db.empty()) {
    print_usage();
    return 1;
  }

  auto schema = parse_schema(schema_arg);
  if (!schema) {
    std::cerr << "  [databento_ingest] unknown --schema '" << schema_arg
              << "' (expected ohlcv1m or tbbo)" << std::endl;
    return 1;
  }

  try {
    DatabentoClient client(out_db, cost_threshold);

    double estimated = client.estimate_cost(dataset, symbol, *schema, start, end);
    std::cout << "  [databento_ingest] estimated cost for " << symbol << " " << schema_arg << " "
              << start << ".." << end << ": $" << estimated << std::endl;

    std::size_t written = client.fetch_and_store(dataset, symbol, *schema, start, end, confirm_cost);
    std::cout << "  [databento_ingest] wrote " << written << " new rows to " << out_db
              << std::endl;
    return 0;
  } catch (const DatabentoCostExceeded& e) {
    std::cerr << "  [databento_ingest] COST GATE BLOCKED THIS REQUEST: " << e.what() << std::endl;
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "  [databento_ingest] error: " << e.what() << std::endl;
    return 1;
  }
}
