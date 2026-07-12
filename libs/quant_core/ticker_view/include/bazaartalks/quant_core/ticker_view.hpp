#pragma once
// Port of ticker_view.py's render_html(): a single-page "screener
// scorecard" HTML view for one ticker, given whatever the warehouse
// currently has for it (fundamentals / DVM technical / DVM composite /
// recent OHLC / the accumulation screen's row). Same pure-function
// pattern as dashboard.py's own render_html: plain data in, an HTML
// string out, unit-testable without a live warehouse.
//
// Scope boundary: `_fetch()` (DuckDB warehouse read) and
// `_fetch_accum_row()` (accumulation_screener call) are I/O, out of
// scope here -- see services/serve_api for the wiring. `main()`'s file-
// write/`--open` CLI is data-plumbing too.
//
// Representation note: Python's `fund`/`dvm`/`comp` are dynamic dicts
// with caller-defined keys (whatever warehouse.ticker_detail() happens to
// return for that market/ticker), and the "recent OHLC" table's columns
// come from `ohlc[0].keys()` -- there is no fixed schema to hang a C++
// struct on. This port represents each as an ordered (key, already-
// stringified value) list instead, matching Python dict iteration order
// exactly; the caller (services/serve_api) is responsible for stringifying
// each value the same way Python's implicit `f"{v}"` would for that
// value's actual type. The ONE place this module does its own numeric
// work, matching the Python original doing its own work at that same
// spot, is the accumulation card's `round(accum_row.get("cmf", 0), 2)`.

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bazaartalks::quant_core {

using KeyValue = std::pair<std::string, std::string>;

struct TickerDetailView {
  // Raw dicts as warehouse.ticker_detail() would return them, INCLUDING
  // "ticker"/"market"/"source" keys -- render_ticker_view_html() itself
  // filters those out when building each table, exactly mirroring
  // Python's own `if k not in (...)` filter, so callers don't need to
  // pre-filter.
  std::vector<KeyValue> fundamentals;   // empty if none cached for this ticker
  std::vector<KeyValue> dvm_technical;  // empty if none cached
  std::vector<KeyValue> dvm_composite;  // empty if none cached
  // Newest-first, matching warehouse.ticker_detail()'s own OHLC ordering
  // (`ohlc` arrives newest-first; the chart reverses it internally, same
  // as the Python original's `chrono = list(reversed(ohlc))`). Each row
  // is itself an ordered (column, value) list; the header row uses the
  // FIRST row's keys, matching `ohlc[0].keys()`.
  std::vector<std::vector<KeyValue>> ohlc;
};

// Port of render_html(). `accum_row`: std::nullopt means no accumulation-
// screen row was available for this ticker (renders "n/a" on the
// Accumulation card) -- a PRESENT-but-empty vector (a row that exists but
// has no "cmf" key) still renders a number, matching
// `accum_row.get("cmf", 0)`; it is NOT the same as nullopt. Note the
// rendered text is the bare integer "0" (not "0.0") in that fieldless
// case specifically -- Python's `.get("cmf", 0)` default is the int 0,
// and `round()` preserves int-ness, so only an ACTUAL cmf value goes
// through float rounding/formatting ("0.13"-style, always with a decimal).
std::string render_ticker_view_html(const std::string& ticker, const std::string& market,
                                    const TickerDetailView& detail,
                                    const std::optional<std::vector<KeyValue>>& accum_row,
                                    const std::string& generated);

}  // namespace bazaartalks::quant_core
