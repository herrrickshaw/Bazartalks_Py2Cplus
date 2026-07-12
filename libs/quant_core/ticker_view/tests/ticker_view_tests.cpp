// Golden value is the ACTUAL byte-for-byte output of ticker_view.py's
// render_html(), captured by running the real Python module directly on
// a synthetic detail dict/accum_row (see the commit message / PR
// description for the capture script) -- not a hand-derived expected
// string.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/quant_core/ticker_view.hpp"

using namespace bazaartalks::quant_core;

namespace {
TickerDetailView make_detail() {
  TickerDetailView d;
  d.fundamentals = {{"ticker", "AAPL"}, {"market", "US"},        {"source", "EDGAR"},
                    {"roe", "28.5"},   {"pe", "31.2"},           {"de", "1.45"},
                    {"rev_growth", "6.1"}};
  d.dvm_technical = {{"ticker", "AAPL"}, {"market", "US"}, {"M", "83.3"}, {"rsi", "65.2"},
                     {"adx", "40.1"}};
  d.dvm_composite = {{"ticker", "AAPL"},
                     {"market", "US"},
                     {"code", "GGG"},
                     {"label", "Strong Performer"},
                     {"composite", "78.5"}};
  d.ohlc = {
      {{"Date", "2026-07-10"}, {"Open", "189.5"}, {"High", "191.2"}, {"Low", "188.9"},
       {"Close", "190.5"}, {"Volume", "52000000"}},
      {{"Date", "2026-07-09"}, {"Open", "187.1"}, {"High", "189.9"}, {"Low", "186.5"},
       {"Close", "189.1"}, {"Volume", "48000000"}},
      {{"Date", "2026-07-08"}, {"Open", "185.0"}, {"High", "188.0"}, {"Low", "184.5"},
       {"Close", "187.0"}, {"Volume", "51000000"}},
  };
  return d;
}
}  // namespace

TEST_CASE("render_ticker_view_html matches ticker_view.py byte-for-byte", "[ticker_view]") {
  auto detail = make_detail();
  std::vector<KeyValue> accum_row = {
      {"ticker", "AAPL"}, {"market", "US"}, {"cmf", "0.1264"}, {"accum", "12.34"}};

  std::string html =
      render_ticker_view_html("AAPL", "US", detail, accum_row, "2026-07-12 10:00");

  std::string expected =
      "<!doctype html><html><head><meta charset='utf-8'><title>AAPL \xc2\xb7 US</title>"
      "<style>\nbody{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:2rem;"
      "color:#1a1a1a;background:#fafafa}\nh1{font-size:1.5rem}h2{font-size:1.1rem;"
      "margin-top:2rem;border-bottom:2px solid #e0e0e0;padding-bottom:.3rem}\ntable{"
      "border-collapse:collapse;width:100%;margin:.5rem 0;background:#fff;font-size:"
      ".85rem}\nth,td{border:1px solid #e5e5e5;padding:.35rem .6rem;text-align:right}\n"
      "th{background:#f0f3f7;text-align:left}td:first-child,th:first-child{text-align:"
      "left}\ntr:nth-child(even){background:#fbfcfd}.meta{color:#888;font-size:.8rem}\n"
      ".badge{display:inline-block;background:#2b6cb0;color:#fff;border-radius:3px;"
      "padding:.1rem .4rem;font-size:.75rem}\n.scorecard{display:flex;flex-wrap:wrap;"
      "gap:.75rem;margin:.5rem 0}\n.card{background:#fff;border:1px solid #e5e5e5;"
      "border-radius:6px;padding:.75rem 1rem;min-width:150px}\n.card .label{font-size:"
      ".75rem;color:#888;text-transform:uppercase}\n.card .value{font-size:1.3rem;"
      "font-weight:600;margin-top:.15rem}\n.card .value.na{color:#bbb;font-weight:400;"
      "font-size:1rem}\n</style></head><body><h1>AAPL <span class='meta'>US</span> "
      "<span class='badge'>ticker view</span></h1><p class='meta'>generated "
      "2026-07-12 10:00</p><h2>Screener scorecard</h2><div class='scorecard'>"
      "<div class=\"card\"><div class=\"label\">DVM composite</div><div class=\"value\">"
      "Strong Performer</div></div><div class=\"card\"><div class=\"label\">Momentum "
      "(M)</div><div class=\"value\">83.3</div></div><div class=\"card\"><div "
      "class=\"label\">RSI</div><div class=\"value\">65.2</div></div><div class=\"card\">"
      "<div class=\"label\">Accumulation (CMF)</div><div class=\"value\">0.13</div></div>"
      "<div class=\"card\"><div class=\"label\">ROE %</div><div class=\"value\">28.5</div>"
      "</div><div class=\"card\"><div class=\"label\">P/E</div><div class=\"value\">31.2"
      "</div></div><div class=\"card\"><div class=\"label\">D/E</div><div class=\"value\">"
      "1.45</div></div></div><h2>Price</h2><svg viewBox='0 0 600 160' width='600' "
      "height='160' xmlns='http://www.w3.org/2000/svg'><text x='24' y='14' "
      "font-size='11' fill='#666'>AAPL close, last 3 bars</text><line x1='24' y1='136' "
      "x2='576' y2='136' stroke='#ccc'/><polygon points='24,136 24.0,136.0 300.0,68.8 "
      "576.0,24.0 576,136' fill='#bcd7ee' opacity='0.5'/><polyline points='24.0,136.0 "
      "300.0,68.8 576.0,24.0' fill='none' stroke='#2b6cb0' stroke-width='2'/><text "
      "x='24' y='154' font-size='9' fill='#666'>2026-07-08</text><text x='576' "
      "y='154' font-size='9' fill='#666' text-anchor='end'>2026-07-10</text><text "
      "x='576' y='24' font-size='9' fill='#666' text-anchor='end'>190.50</text><text "
      "x='576' y='134' font-size='9' fill='#666' text-anchor='end'>187.00</text></svg>"
      "<div style='display:flex;gap:1rem;flex-wrap:wrap'><svg viewBox='0 0 160 54' "
      "width='160' height='54' xmlns='http://www.w3.org/2000/svg'><text x='4' y='14' "
      "font-size='13' font-weight='600' fill='#1a1a1a'>83</text><rect x='4' y='24' "
      "width='152' height='14' rx='3' fill='#e5e5e5'/><rect x='4' y='24' width='126.6' "
      "height='14' rx='3' fill='#1e8449'/><text x='4' y='50' font-size='9' "
      "fill='#666'>Momentum (M)</text></svg><svg viewBox='0 0 160 54' width='160' "
      "height='54' xmlns='http://www.w3.org/2000/svg'><text x='4' y='14' "
      "font-size='13' font-weight='600' fill='#1a1a1a'>78</text><rect x='4' y='24' "
      "width='152' height='14' rx='3' fill='#e5e5e5'/><rect x='4' y='24' width='119.3' "
      "height='14' rx='3' fill='#1e8449'/><text x='4' y='50' font-size='9' "
      "fill='#666'>DVM composite score</text></svg></div><h2>Fundamentals <span "
      "class='badge'>via EDGAR</span></h2><table><tr><td>roe</td><td>28.5</td></tr>"
      "<tr><td>pe</td><td>31.2</td></tr><tr><td>de</td><td>1.45</td></tr><tr>"
      "<td>rev_growth</td><td>6.1</td></tr></table><h2>DVM technical &amp; composite "
      "classification</h2><table><tr><td>tech_M</td><td>83.3</td></tr><tr>"
      "<td>tech_rsi</td><td>65.2</td></tr><tr><td>tech_adx</td><td>40.1</td></tr><tr>"
      "<td>composite_code</td><td>GGG</td></tr><tr><td>composite_label</td>"
      "<td>Strong Performer</td></tr><tr><td>composite_composite</td><td>78.5</td>"
      "</tr></table><h2>Recent OHLC (3 bars)</h2><table><tr><th>Date</th><th>Open</th>"
      "<th>High</th><th>Low</th><th>Close</th><th>Volume</th></tr><tr>"
      "<td>2026-07-10</td><td>189.5</td><td>191.2</td><td>188.9</td><td>190.5</td>"
      "<td>52000000</td></tr><tr><td>2026-07-09</td><td>187.1</td><td>189.9</td>"
      "<td>186.5</td><td>189.1</td><td>48000000</td></tr><tr><td>2026-07-08</td>"
      "<td>185.0</td><td>188.0</td><td>184.5</td><td>187.0</td><td>51000000</td>"
      "</tr></table></body></html>";

  CHECK(html == expected);
}

TEST_CASE("render_ticker_view_html shows n/a for a missing accumulation row (nullopt), "
          "not for a present-but-fieldless one",
          "[ticker_view]") {
  auto detail = make_detail();

  std::string with_none = render_ticker_view_html("AAPL", "US", detail, std::nullopt, "now");
  CHECK(with_none.find("<div class=\"label\">Accumulation (CMF)</div>"
                       "<div class=\"value na\">n/a</div>") != std::string::npos);

  std::vector<KeyValue> row_no_cmf = {{"ticker", "AAPL"}, {"market", "US"}};
  std::string with_row = render_ticker_view_html("AAPL", "US", detail, row_no_cmf, "now");
  // python3: round({}.get("cmf", 0), 2) -> the bare int 0 (str "0"), NOT
  // "0.0" -- .get()'s default is the Python int 0, and round() preserves
  // int-ness. A real cmf value goes through float rounding instead.
  CHECK(with_row.find("<div class=\"label\">Accumulation (CMF)</div>"
                      "<div class=\"value\">0</div>") != std::string::npos);
}

TEST_CASE("render_ticker_view_html shows placeholders when fundamentals/DVM/OHLC are all "
          "absent",
          "[ticker_view]") {
  TickerDetailView empty;
  std::string html = render_ticker_view_html("XYZ", "US", empty, std::nullopt, "now");
  CHECK(html.find("no OHLC history to chart") != std::string::npos);
  CHECK(html.find("no fundamentals cached for this ticker") != std::string::npos);
  CHECK(html.find("no DVM data cached for this ticker") != std::string::npos);
  CHECK(html.find("no OHLC history for this ticker/market") != std::string::npos);
  // No "M"/"composite" -> no gauge block at all.
  CHECK(html.find("<svg") == std::string::npos);
}

TEST_CASE("render_ticker_view_html degrades gracefully, not a crash, on a malformed "
          "numeric field from an upstream caller",
          "[ticker_view]") {
  // This module's contract documents that callers supply pre-stringified
  // values matching Python's str() output for that field's actual type --
  // a well-formed caller never hits this path. This test locks in the
  // defensive fallback (a NaN-driven but still-rendered page) against a
  // malformed caller instead, since a serving layer degrading one bad
  // upstream value is preferable to an uncaught std::invalid_argument
  // taking the whole request down.
  TickerDetailView detail;
  detail.dvm_technical = {{"M", "not-a-number"}};
  detail.dvm_composite = {{"composite", "also-not-a-number"}};
  detail.ohlc = {{{"Date", "2026-07-12"}, {"Close", "garbage"}}};

  std::string html;
  CHECK_NOTHROW(html = render_ticker_view_html("XYZ", "US", detail, std::nullopt, "now"));
  CHECK(!html.empty());
  // The scorecard still shows the raw (malformed) string as-is -- card()
  // only checks presence, not parseability.
  CHECK(html.find("<div class=\"value\">not-a-number</div>") != std::string::npos);
}
