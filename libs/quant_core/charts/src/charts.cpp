#include "bazaartalks/quant_core/charts.hpp"

#include <algorithm>
#include <cstdio>

namespace bazaartalks::quant_core {

namespace {
constexpr const char* kStroke = "#2b6cb0";
constexpr const char* kFill = "#bcd7ee";
constexpr const char* kAxis = "#ccc";
constexpr const char* kText = "#666";

std::string fixed(double x, int decimals) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", decimals, x);
  return std::string(buf);
}

std::vector<double> scale(const std::vector<double>& values, double lo, double hi,
                          double out_lo, double out_hi) {
  double span = (hi - lo) != 0.0 ? (hi - lo) : 1.0;
  std::vector<double> out(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    out[i] = out_lo + (values[i] - lo) / span * (out_hi - out_lo);
  }
  return out;
}
}  // namespace

std::string line_chart(const std::vector<std::string>& labels, const std::vector<double>& values,
                       int width, int height, const std::string& title) {
  if (values.empty()) return "<p class='meta'>no data</p>";

  int pad = 24;
  double lo = *std::min_element(values.begin(), values.end());
  double hi = *std::max_element(values.begin(), values.end());

  std::vector<double> idx(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) idx[i] = static_cast<double>(i);
  double idx_hi = std::max(1.0, static_cast<double>(values.size()) - 1.0);
  std::vector<double> xs = scale(idx, 0.0, idx_hi, pad, width - pad);
  std::vector<double> ys = scale(values, lo, hi, height - pad, pad);  // invert

  std::string points;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) points += " ";
    points += fixed(xs[i], 1) + "," + fixed(ys[i], 1);
  }
  std::string area = std::to_string(pad) + "," + std::to_string(height - pad) + " " + points +
                     " " + std::to_string(width - pad) + "," + std::to_string(height - pad);

  std::string title_svg;
  if (!title.empty()) {
    title_svg = "<text x='" + std::to_string(pad) + "' y='14' font-size='11' fill='" +
               kText + "'>" + title + "</text>";
  }
  std::string first_lbl = labels.empty() ? "" : labels.front();
  std::string last_lbl = labels.empty() ? "" : labels.back();

  return "<svg viewBox='0 0 " + std::to_string(width) + " " + std::to_string(height) +
        "' width='" + std::to_string(width) + "' height='" + std::to_string(height) +
        "' xmlns='http://www.w3.org/2000/svg'>" + title_svg + "<line x1='" +
        std::to_string(pad) + "' y1='" + std::to_string(height - pad) + "' x2='" +
        std::to_string(width - pad) + "' y2='" + std::to_string(height - pad) + "' stroke='" +
        kAxis + "'/>" + "<polygon points='" + area + "' fill='" + kFill + "' opacity='0.5'/>" +
        "<polyline points='" + points + "' fill='none' stroke='" + kStroke +
        "' stroke-width='2'/>" + "<text x='" + std::to_string(pad) + "' y='" +
        std::to_string(height - 6) + "' font-size='9' fill='" + kText + "'>" + first_lbl +
        "</text>" + "<text x='" + std::to_string(width - pad) + "' y='" +
        std::to_string(height - 6) + "' font-size='9' fill='" + kText +
        "' text-anchor='end'>" + last_lbl + "</text>" + "<text x='" +
        std::to_string(width - pad) + "' y='" + std::to_string(pad) +
        "' font-size='9' fill='" + kText + "' text-anchor='end'>" + fixed(hi, 2) + "</text>" +
        "<text x='" + std::to_string(width - pad) + "' y='" + std::to_string(height - pad - 2) +
        "' font-size='9' fill='" + kText + "' text-anchor='end'>" + fixed(lo, 2) + "</text>" +
        "</svg>";
}

std::string bar_chart(const std::vector<std::string>& labels, const std::vector<long long>& values,
                      int width, int height, const std::string& title) {
  if (values.empty()) return "<p class='meta'>no data</p>";

  // plot_w/plot_h are plain ints in the Python original (width/height/pad
  // are always ints) -- keeping them int here, not double, matters
  // because they're interpolated bare (no .1f) into the axis-line y1/y2
  // and the bar-label y coordinate below.
  int pad_l = 32, pad_b = 40, pad_t = 24;
  int plot_w = width - pad_l - 10;
  int plot_h = height - pad_b - pad_t;
  long long max_v = *std::max_element(values.begin(), values.end());
  double hi = (max_v == 0) ? 1.0 : static_cast<double>(max_v);
  std::size_t n = values.size();
  double bw = static_cast<double>(plot_w) / static_cast<double>(n) * 0.7;
  double gap = static_cast<double>(plot_w) / static_cast<double>(n);

  std::string title_svg;
  if (!title.empty()) {
    title_svg = "<text x='" + std::to_string(pad_l) + "' y='14' font-size='11' fill='" +
               kText + "'>" + title + "</text>";
  }

  std::string bars;
  for (std::size_t i = 0; i < n; ++i) {
    double v = static_cast<double>(values[i]);
    double bh = (v / hi) * plot_h;
    double x = pad_l + static_cast<double>(i) * gap + (gap - bw) / 2.0;
    double y = pad_t + (plot_h - bh);
    bars += "<rect x='" + fixed(x, 1) + "' y='" + fixed(y, 1) + "' width='" + fixed(bw, 1) +
           "' height='" + fixed(bh, 1) + "' fill='" + kStroke + "'/>";
    bars += "<text x='" + fixed(x + bw / 2.0, 1) + "' y='" + std::to_string(pad_t + plot_h + 12) +
           "' font-size='9' fill='" + kText + "' text-anchor='middle'>" +
           (i < labels.size() ? labels[i] : std::string()) + "</text>";
    bars += "<text x='" + fixed(x + bw / 2.0, 1) + "' y='" + fixed(y - 3, 1) +
           "' font-size='9' fill='" + kText + "' text-anchor='middle'>" +
           std::to_string(values[i]) + "</text>";
  }

  return "<svg viewBox='0 0 " + std::to_string(width) + " " + std::to_string(height) +
        "' width='" + std::to_string(width) + "' height='" + std::to_string(height) +
        "' xmlns='http://www.w3.org/2000/svg'>" + title_svg + "<line x1='" +
        std::to_string(pad_l) + "' y1='" + std::to_string(pad_t + plot_h) + "' x2='" +
        std::to_string(width - 10) + "' y2='" + std::to_string(pad_t + plot_h) + "' stroke='" +
        kAxis + "'/>" + bars + "</svg>";
}

std::string gauge(double value, double lo, double hi, const std::string& label, int width,
                  int height) {
  value = std::max(lo, std::min(hi, value));
  double denom = (hi - lo) != 0.0 ? (hi - lo) : 1.0;
  double frac = (value - lo) / denom;
  std::string color = frac < 0.34 ? "#c0392b" : (frac < 0.67 ? "#d68910" : "#1e8449");

  int bar_x = 4, bar_y = 24, bar_h = 14;
  int bar_w = width - 8;
  double fill_w = bar_w * frac;

  return "<svg viewBox='0 0 " + std::to_string(width) + " " + std::to_string(height) +
        "' width='" + std::to_string(width) + "' height='" + std::to_string(height) +
        "' xmlns='http://www.w3.org/2000/svg'>" + "<text x='" + std::to_string(bar_x) +
        "' y='14' font-size='13' font-weight='600' fill='#1a1a1a'>" + fixed(value, 0) +
        "</text>" + "<rect x='" + std::to_string(bar_x) + "' y='" + std::to_string(bar_y) +
        "' width='" + std::to_string(bar_w) + "' height='" + std::to_string(bar_h) +
        "' rx='3' fill='#e5e5e5'/>" + "<rect x='" + std::to_string(bar_x) + "' y='" +
        std::to_string(bar_y) + "' width='" + fixed(fill_w, 1) + "' height='" +
        std::to_string(bar_h) + "' rx='3' fill='" + color + "'/>" + "<text x='" +
        std::to_string(bar_x) + "' y='" + std::to_string(bar_y + bar_h + 12) +
        "' font-size='9' fill='" + kText + "'>" + label + "</text>" + "</svg>";
}

}  // namespace bazaartalks::quant_core
