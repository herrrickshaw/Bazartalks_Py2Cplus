#include "bazaartalks/quant_core/ticker_view.hpp"

#include "bazaartalks/quant_core/charts.hpp"

#include <cmath>
#include <set>

namespace bazaartalks::quant_core {

namespace {

constexpr const char* kCss =
    "\nbody{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:2rem;color:#1a1a1a;"
    "background:#fafafa}\nh1{font-size:1.5rem}h2{font-size:1.1rem;margin-top:2rem;"
    "border-bottom:2px solid #e0e0e0;padding-bottom:.3rem}\ntable{border-collapse:collapse;"
    "width:100%;margin:.5rem 0;background:#fff;font-size:.85rem}\nth,td{border:1px solid "
    "#e5e5e5;padding:.35rem .6rem;text-align:right}\nth{background:#f0f3f7;text-align:left}"
    "td:first-child,th:first-child{text-align:left}\ntr:nth-child(even){background:#fbfcfd}"
    ".meta{color:#888;font-size:.8rem}\n.badge{display:inline-block;background:#2b6cb0;"
    "color:#fff;border-radius:3px;padding:.1rem .4rem;font-size:.75rem}\n.scorecard{display:"
    "flex;flex-wrap:wrap;gap:.75rem;margin:.5rem 0}\n.card{background:#fff;border:1px solid "
    "#e5e5e5;border-radius:6px;padding:.75rem 1rem;min-width:150px}\n.card .label{font-size:"
    ".75rem;color:#888;text-transform:uppercase}\n.card .value{font-size:1.3rem;font-weight:"
    "600;margin-top:.15rem}\n.card .value.na{color:#bbb;font-weight:400;font-size:1rem}\n";

std::optional<std::string> find(const std::vector<KeyValue>& kvs, const std::string& key) {
  for (const auto& [k, v] : kvs) {
    if (k == key) return v;
  }
  return std::nullopt;
}

// Python's `a.get(x) or a.get(y)`: truthy fallback (an empty string is
// falsy), not a presence check.
std::optional<std::string> truthy_or(const std::optional<std::string>& a,
                                     const std::optional<std::string>& b) {
  if (a.has_value() && !a->empty()) return a;
  if (b.has_value() && !b->empty()) return b;
  return std::nullopt;
}

std::string card(const std::string& label, const std::optional<std::string>& value) {
  if (!value.has_value()) {
    return "<div class=\"card\"><div class=\"label\">" + label +
          "</div><div class=\"value na\">n/a</div></div>";
  }
  return "<div class=\"card\"><div class=\"label\">" + label + "</div><div class=\"value\">" +
        *value + "</div></div>";
}

double round2(double x) { return std::round(x * 100.0) / 100.0; }

// Matches Python's `str(round(x, 2))`: a float always renders with at
// least one decimal digit (never bare "5"), and round(x,2)'s result
// always needs at most 2 decimal digits to round-trip -- so formatting
// with exactly 2 decimals then stripping ONE trailing zero (never both)
// reproduces Python's shortest-round-trip float repr for this specific
// case (round(x,2) results only, not floats in general).
std::string format_round2(double x) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f", round2(x));
  std::string s(buf);
  if (!s.empty() && s.back() == '0') s.pop_back();
  return s;
}

}  // namespace

std::string render_ticker_view_html(const std::string& ticker, const std::string& market,
                                    const TickerDetailView& detail,
                                    const std::optional<std::vector<KeyValue>>& accum_row,
                                    const std::string& generated) {
  const auto& fund = detail.fundamentals;
  const auto& dvm = detail.dvm_technical;
  const auto& comp = detail.dvm_composite;
  const auto& ohlc = detail.ohlc;

  std::string html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<title>" + ticker + " \xc2\xb7 " + market + "</title><style>" + kCss +
         "</style></head><body>";
  html += "<h1>" + ticker + " <span class='meta'>" + market +
         "</span> <span class='badge'>ticker view</span></h1>";
  html += "<p class='meta'>generated " + generated + "</p>";

  html += "<h2>Screener scorecard</h2><div class='scorecard'>";
  html += card("DVM composite", truthy_or(find(comp, "label"), find(comp, "code")));
  auto m_val = find(dvm, "M");
  html += card("Momentum (M)", m_val);
  html += card("RSI", find(dvm, "rsi"));
  if (!accum_row.has_value()) {
    html += card("Accumulation (CMF)", std::nullopt);
  } else {
    auto cmf = find(*accum_row, "cmf");
    // `.get("cmf", 0)`'s default is the Python INT 0, not 0.0 -- `round()`
    // preserves int-ness, so a present-but-fieldless row renders the bare
    // int "0" here, NOT the float-style "0.0" a real cmf value would get.
    html += card("Accumulation (CMF)",
                cmf.has_value() ? format_round2(std::stod(*cmf)) : std::string("0"));
  }
  html += card("ROE %", find(fund, "roe"));
  html += card("P/E", find(fund, "pe"));
  html += card("D/E", find(fund, "de"));
  html += "</div>";

  html += "<h2>Price</h2>";
  if (!ohlc.empty()) {
    std::vector<std::string> dates;
    std::vector<double> closes;
    for (auto it = ohlc.rbegin(); it != ohlc.rend(); ++it) {
      auto date_str = find(*it, "Date");
      dates.push_back(date_str.has_value() ? date_str->substr(0, 10) : "");
      auto close_str = find(*it, "Close");
      closes.push_back(close_str.has_value() ? std::stod(*close_str) : 0.0);
    }
    html += line_chart(dates, closes, 600, 160,
                       ticker + " close, last " + std::to_string(closes.size()) + " bars");
  } else {
    html += "<p class='meta'>no OHLC history to chart</p>";
  }

  auto composite_val = find(comp, "composite");
  if (m_val.has_value() || composite_val.has_value()) {
    html += "<div style='display:flex;gap:1rem;flex-wrap:wrap'>";
    if (m_val.has_value()) html += gauge(std::stod(*m_val), 0, 100, "Momentum (M)");
    if (composite_val.has_value()) {
      html += gauge(std::stod(*composite_val), 0, 100, "DVM composite score");
    }
    html += "</div>";
  }

  auto src = find(fund, "source");
  std::string src_badge =
      (src.has_value() && !src->empty()) ? " <span class='badge'>via " + *src + "</span>" : "";
  html += "<h2>Fundamentals" + src_badge + "</h2>";
  if (!fund.empty()) {
    static const std::set<std::string> kSkip = {"ticker", "market", "source"};
    std::string rows;
    for (const auto& [k, v] : fund) {
      if (kSkip.count(k)) continue;
      rows += "<tr><td>" + k + "</td><td>" + v + "</td></tr>";
    }
    html += "<table>" + rows + "</table>";
  } else {
    html += "<p class='meta'>no fundamentals cached for this ticker</p>";
  }

  html += "<h2>DVM technical &amp; composite classification</h2>";
  {
    static const std::set<std::string> kSkip = {"ticker", "market"};
    std::string rows;
    for (const auto& [k, v] : dvm) {
      if (kSkip.count(k)) continue;
      rows += "<tr><td>tech_" + k + "</td><td>" + v + "</td></tr>";
    }
    for (const auto& [k, v] : comp) {
      if (kSkip.count(k)) continue;
      rows += "<tr><td>composite_" + k + "</td><td>" + v + "</td></tr>";
    }
    if (!rows.empty()) {
      html += "<table>" + rows + "</table>";
    } else {
      html += "<p class='meta'>no DVM data cached for this ticker</p>";
    }
  }

  html += "<h2>Recent OHLC (" + std::to_string(ohlc.size()) + " bars)</h2>";
  if (!ohlc.empty()) {
    std::string head;
    for (const auto& [k, v] : ohlc.front()) head += "<th>" + k + "</th>";
    std::string body;
    for (const auto& row : ohlc) {
      body += "<tr>";
      for (const auto& [k, v] : row) body += "<td>" + v + "</td>";
      body += "</tr>";
    }
    html += "<table><tr>" + head + "</tr>" + body + "</table>";
  } else {
    html += "<p class='meta'>no OHLC history for this ticker/market</p>";
  }

  html += "</body></html>";
  return html;
}

}  // namespace bazaartalks::quant_core
