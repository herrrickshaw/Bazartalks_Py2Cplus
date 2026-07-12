#include "bazaartalks/storage/wide_frame.hpp"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>

#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <stdexcept>

namespace bazaartalks::storage {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

[[noreturn]] void raise(const arrow::Status& status, const std::string& context) {
  throw std::runtime_error(context + ": " + status.ToString());
}

std::shared_ptr<arrow::Table> read_table(const std::string& path) {
  auto infile_result = arrow::io::ReadableFile::Open(path);
  if (!infile_result.ok()) raise(infile_result.status(), "failed to open parquet file " + path);
  auto infile = *infile_result;

  auto reader_result = parquet::arrow::OpenFile(infile, arrow::default_memory_pool());
  if (!reader_result.ok()) raise(reader_result.status(), "failed to open parquet reader for " + path);
  auto reader = std::move(*reader_result);

  auto table_result = reader->ReadTable();
  if (!table_result.ok()) raise(table_result.status(), "failed to read parquet table from " + path);
  std::shared_ptr<arrow::Table> table = *table_result;

  auto combined = table->CombineChunks();
  if (!combined.ok()) raise(combined.status(), "failed to combine chunks for " + path);
  return *combined;
}

// A date::year_month_day date, extracted from either a Date32 (days since
// epoch) or Timestamp (any unit) Arrow column -- pandas.to_parquet writes
// datetime64[ns] columns as Arrow Timestamp(ns), so both are handled.
date::year_month_day extract_date(const std::shared_ptr<arrow::Array>& date_col, int64_t row) {
  if (auto d32 = std::dynamic_pointer_cast<arrow::Date32Array>(date_col)) {
    return date::year_month_day{date::sys_days{date::days{d32->Value(row)}}};
  }
  if (auto ts = std::dynamic_pointer_cast<arrow::TimestampArray>(date_col)) {
    auto type = std::static_pointer_cast<arrow::TimestampType>(ts->type());
    int64_t raw = ts->Value(row);
    int64_t days_since_epoch = 0;
    switch (type->unit()) {
      case arrow::TimeUnit::SECOND:
        days_since_epoch = raw / 86400;
        break;
      case arrow::TimeUnit::MILLI:
        days_since_epoch = raw / 86400000LL;
        break;
      case arrow::TimeUnit::MICRO:
        days_since_epoch = raw / 86400000000LL;
        break;
      case arrow::TimeUnit::NANO:
        days_since_epoch = raw / 86400000000000LL;
        break;
    }
    return date::year_month_day{date::sys_days{date::days{days_since_epoch}}};
  }
  throw std::runtime_error("Date column is neither Date32 nor Timestamp");
}

std::string extract_symbol(const std::shared_ptr<arrow::Array>& symbol_col, int64_t row) {
  if (auto s = std::dynamic_pointer_cast<arrow::StringArray>(symbol_col)) {
    return s->GetString(row);
  }
  if (auto s = std::dynamic_pointer_cast<arrow::LargeStringArray>(symbol_col)) {
    return s->GetString(row);
  }
  throw std::runtime_error("Symbol column is neither String nor LargeString");
}

// Manual widening to double -- avoids pulling in Arrow's whole compute
// module (arrow::compute::Cast) just for this. Volume-like columns are
// commonly int64 in a parquet written from a pandas int dtype; Close-like
// columns are float64. Matches wide()'s `.astype(float)`.
double extract_double(const std::shared_ptr<arrow::Array>& col, int64_t row) {
  if (col->IsNull(row)) return kNaN;
  if (auto a = std::dynamic_pointer_cast<arrow::DoubleArray>(col)) return a->Value(row);
  if (auto a = std::dynamic_pointer_cast<arrow::FloatArray>(col))
    return static_cast<double>(a->Value(row));
  if (auto a = std::dynamic_pointer_cast<arrow::Int64Array>(col))
    return static_cast<double>(a->Value(row));
  if (auto a = std::dynamic_pointer_cast<arrow::Int32Array>(col))
    return static_cast<double>(a->Value(row));
  throw std::runtime_error("value column is not a recognized numeric Arrow type");
}

}  // namespace

std::optional<double> WideFrame::at(std::size_t date_idx, std::size_t symbol_idx) const {
  double v = values(static_cast<Eigen::Index>(date_idx), static_cast<Eigen::Index>(symbol_idx));
  if (std::isnan(v)) return std::nullopt;
  return v;
}

std::optional<WideFrame> pivot_long_parquet(const std::string& parquet_path,
                                             const std::string& field) {
  if (!std::filesystem::exists(parquet_path)) return std::nullopt;

  auto table = read_table(parquet_path);

  auto date_col = table->GetColumnByName("Date");
  auto symbol_col = table->GetColumnByName("Symbol");
  auto field_col = table->GetColumnByName(field);
  if (!date_col || !symbol_col || !field_col) {
    throw std::runtime_error("parquet file " + parquet_path +
                              " is missing one of Date/Symbol/" + field);
  }

  auto value_array = field_col->chunk(0);
  auto date_array = date_col->chunk(0);
  auto symbol_array = symbol_col->chunk(0);

  // aggfunc="last": iterate rows in file order, last write per (date,
  // symbol) key wins.
  std::map<date::year_month_day, std::size_t> date_index;   // sorted by key (std::map)
  std::map<std::string, std::size_t> symbol_index;          // sorted by key
  std::vector<std::tuple<date::year_month_day, std::string, double>> rows;
  rows.reserve(static_cast<std::size_t>(table->num_rows()));

  for (int64_t i = 0; i < table->num_rows(); ++i) {
    if (date_array->IsNull(i) || symbol_array->IsNull(i)) continue;
    date::year_month_day d = extract_date(date_array, i);
    std::string sym = extract_symbol(symbol_array, i);
    double v = extract_double(value_array, i);
    date_index.emplace(d, 0);
    symbol_index.emplace(sym, 0);
    rows.emplace_back(d, sym, v);
  }

  WideFrame out;
  out.dates.reserve(date_index.size());
  std::size_t idx = 0;
  for (auto& [d, i] : date_index) {
    i = idx++;
    out.dates.push_back(d);
  }
  out.symbols.reserve(symbol_index.size());
  idx = 0;
  for (auto& [s, i] : symbol_index) {
    i = idx++;
    out.symbols.push_back(s);
  }

  out.values = Eigen::MatrixXd::Constant(static_cast<Eigen::Index>(out.dates.size()),
                                         static_cast<Eigen::Index>(out.symbols.size()), kNaN);
  for (const auto& [d, sym, v] : rows) {
    out.values(static_cast<Eigen::Index>(date_index[d]),
               static_cast<Eigen::Index>(symbol_index[sym])) = v;  // last write wins
  }

  return out;
}

}  // namespace bazaartalks::storage
