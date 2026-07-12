#pragma once
// Port of charts.py -- pure, dependency-free SVG string generators (no
// graphics library), matching the platform's "dependency-free... unit-
// testable and importable anywhere" ethos. Each function takes plain data
// and returns a raw `<svg>...</svg>` string, byte-for-byte matching the
// Python original's f-string formatting (single-quoted attributes, `.1f`/
// `.0f`-style fixed-decimal number formatting) so it's a genuine drop-in
// for whatever embeds it (services/serve_api's `/chart/ticker/{symbol}`,
// the ticker_view/dashboard HTML renderers).
//
// `main()`'s `--demo` CLI (writes a demo HTML file) is not ported --
// data-plumbing, same boundary as every other phase's `main()`.

#include <cstddef>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// Port of line_chart(): an OHLC-close-style line chart. Returns
// "<p class='meta'>no data</p>" if `values` is empty, matching the
// Python original's own empty-data guard. `labels` (dates/x-axis) is only
// used for the first/last tick text, exactly like the Python original --
// a `labels` shorter than `values` is fine (only index 0 and back() are
// read), matching Python's `labels[0]`/`labels[-1]` (which would actually
// IndexError on an empty `labels` list with non-empty `values` -- this
// port instead falls back to "" for that case, a deliberate divergence
// from a Python crash toward a safe default, not a byte-for-byte replica
// of an error path no caller relies on).
std::string line_chart(const std::vector<std::string>& labels, const std::vector<double>& values,
                       int width = 480, int height = 120, const std::string& title = "");

// Port of bar_chart(): a simple vertical bar chart (e.g. DVM
// classification counts). `values` is `long long`, not `double` --
// matching every real call site (dashboard.py's `count(*) n` column, the
// module's own `--demo` int list), and Python's plain `{v}` text
// formatting for an int (no decimal point) rather than a float's.
std::string bar_chart(const std::vector<std::string>& labels, const std::vector<long long>& values,
                      int width = 480, int height = 220, const std::string& title = "");

// Port of gauge(): a horizontal bar gauge for one bounded metric.
// `value` is clamped to [lo, hi] first, matching
// `value = max(lo, min(hi, value))`. Color bands: red below 34% of the
// range, amber 34-67%, green above 67% -- exact Python thresholds
// (`frac < 0.34` / `frac < 0.67`), not "roughly a third".
std::string gauge(double value, double lo = 0.0, double hi = 100.0, const std::string& label = "",
                  int width = 160, int height = 54);

}  // namespace bazaartalks::quant_core
