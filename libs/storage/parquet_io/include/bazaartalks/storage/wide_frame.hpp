#pragma once
// Port of marketdata.py's wide()/close_volume(): reads a long-format
// cleaned_long_<market>.parquet (columns: Date, Symbol, plus OHLCV fields)
// and pivots one field into a dense Date x Symbol matrix, matching
// `px.pivot_table(index="Date", columns="Symbol", values=field,
// aggfunc="last")` -- if a (Date, Symbol) pair repeats, the LAST row for
// that pair wins (not first, not an aggregate), since that's pandas'
// aggfunc="last" semantics.
//
// Uses Apache Arrow's C++ Parquet reader directly -- pyarrow (which
// pandas' read_parquet uses under the hood) and this are the same
// underlying library, so there is no schema-translation risk here.

#include <Eigen/Dense>
#include <date/date.h>

#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::storage {

// A dense Date x Symbol matrix. Missing (date, symbol) combinations are
// NaN, matching pivot_table's default fill. `dates`/`symbols` are the row/
// column labels, both sorted ascending (pandas' pivot_table sorts both
// axes by default).
struct WideFrame {
  std::vector<date::year_month_day> dates;
  std::vector<std::string> symbols;
  Eigen::MatrixXd values;  // dates.size() x symbols.size()

  // Returns the value at (date_idx, symbol_idx), or nullopt if it's NaN
  // (i.e. that (date, symbol) pair had no row in the source parquet).
  std::optional<double> at(std::size_t date_idx, std::size_t symbol_idx) const;
};

// Reads `parquet_path` (a long-format cleaned_long_<market>.parquet) and
// pivots `field` (e.g. "Close", "Volume") to a WideFrame. Returns nullopt
// if the file doesn't exist, matching wide()'s "return None" on a missing
// parquet rather than throwing.
std::optional<WideFrame> pivot_long_parquet(const std::string& parquet_path,
                                             const std::string& field);

}  // namespace bazaartalks::storage
