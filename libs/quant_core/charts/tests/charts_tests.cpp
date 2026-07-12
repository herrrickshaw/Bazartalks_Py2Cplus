// Golden values are the ACTUAL byte-for-byte output of charts.py's
// line_chart()/bar_chart()/gauge(), captured by running the real Python
// module directly (see the commit message / PR description for the
// capture script) -- not hand-derived expected strings.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/quant_core/charts.hpp"

using namespace bazaartalks::quant_core;

TEST_CASE("line_chart matches charts.py byte-for-byte on the module's own demo series",
          "[charts]") {
  std::vector<std::string> dates;
  for (int i = 0; i < 20; ++i) dates.push_back("d" + std::to_string(i));
  std::vector<double> values = {100, 102, 101, 105, 108, 107, 110, 112, 109, 111,
                                115, 117, 116, 120, 119, 122, 125, 123, 126, 128};

  std::string svg = line_chart(dates, values, 480, 120, "Demo close price");
  std::string expected =
      "<svg viewBox='0 0 480 120' width='480' height='120' "
      "xmlns='http://www.w3.org/2000/svg'><text x='24' y='14' font-size='11' "
      "fill='#666'>Demo close price</text><line x1='24' y1='96' x2='456' y2='96' "
      "stroke='#ccc'/><polygon points='24,96 24.0,96.0 46.7,90.9 69.5,93.4 92.2,83.1 "
      "114.9,75.4 137.7,78.0 160.4,70.3 183.2,65.1 205.9,72.9 228.6,67.7 251.4,57.4 "
      "274.1,52.3 296.8,54.9 319.6,44.6 342.3,47.1 365.1,39.4 387.8,31.7 410.5,36.9 "
      "433.3,29.1 456.0,24.0 456,96' fill='#bcd7ee' opacity='0.5'/><polyline "
      "points='24.0,96.0 46.7,90.9 69.5,93.4 92.2,83.1 114.9,75.4 137.7,78.0 160.4,70.3 "
      "183.2,65.1 205.9,72.9 228.6,67.7 251.4,57.4 274.1,52.3 296.8,54.9 319.6,44.6 "
      "342.3,47.1 365.1,39.4 387.8,31.7 410.5,36.9 433.3,29.1 456.0,24.0' fill='none' "
      "stroke='#2b6cb0' stroke-width='2'/><text x='24' y='114' font-size='9' "
      "fill='#666'>d0</text><text x='456' y='114' font-size='9' fill='#666' "
      "text-anchor='end'>d19</text><text x='456' y='24' font-size='9' fill='#666' "
      "text-anchor='end'>128.00</text><text x='456' y='94' font-size='9' fill='#666' "
      "text-anchor='end'>100.00</text></svg>";
  CHECK(svg == expected);
}

TEST_CASE("line_chart returns the no-data placeholder for an empty series", "[charts]") {
  CHECK(line_chart({}, {}) == "<p class='meta'>no data</p>");
}

TEST_CASE("bar_chart matches charts.py byte-for-byte on the DVM classification demo",
          "[charts]") {
  std::string svg =
      bar_chart({"GGG", "GGB", "BBG", "BBB"}, {42, 18, 9, 3}, 480, 220, "DVM classification");
  std::string expected =
      "<svg viewBox='0 0 480 220' width='480' height='220' "
      "xmlns='http://www.w3.org/2000/svg'><text x='32' y='14' font-size='11' "
      "fill='#666'>DVM classification</text><line x1='32' y1='180' x2='470' y2='180' "
      "stroke='#ccc'/><rect x='48.4' y='24.0' width='76.6' height='156.0' "
      "fill='#2b6cb0'/><text x='86.8' y='192' font-size='9' fill='#666' "
      "text-anchor='middle'>GGG</text><text x='86.8' y='21.0' font-size='9' fill='#666' "
      "text-anchor='middle'>42</text><rect x='157.9' y='113.1' width='76.6' "
      "height='66.9' fill='#2b6cb0'/><text x='196.2' y='192' font-size='9' fill='#666' "
      "text-anchor='middle'>GGB</text><text x='196.2' y='110.1' font-size='9' "
      "fill='#666' text-anchor='middle'>18</text><rect x='267.4' y='146.6' "
      "width='76.6' height='33.4' fill='#2b6cb0'/><text x='305.8' y='192' "
      "font-size='9' fill='#666' text-anchor='middle'>BBG</text><text x='305.8' "
      "y='143.6' font-size='9' fill='#666' text-anchor='middle'>9</text><rect "
      "x='376.9' y='168.9' width='76.6' height='11.1' fill='#2b6cb0'/><text "
      "x='415.2' y='192' font-size='9' fill='#666' text-anchor='middle'>BBB</text>"
      "<text x='415.2' y='165.9' font-size='9' fill='#666' "
      "text-anchor='middle'>3</text></svg>";
  CHECK(svg == expected);
}

TEST_CASE("bar_chart returns the no-data placeholder for an empty series", "[charts]") {
  CHECK(bar_chart({}, {}) == "<p class='meta'>no data</p>");
}

TEST_CASE("gauge matches charts.py byte-for-byte at 72/100 (green band)", "[charts]") {
  std::string svg = gauge(72, 0, 100, "Momentum (M)");
  std::string expected =
      "<svg viewBox='0 0 160 54' width='160' height='54' "
      "xmlns='http://www.w3.org/2000/svg'><text x='4' y='14' font-size='13' "
      "font-weight='600' fill='#1a1a1a'>72</text><rect x='4' y='24' width='152' "
      "height='14' rx='3' fill='#e5e5e5'/><rect x='4' y='24' width='109.4' "
      "height='14' rx='3' fill='#1e8449'/><text x='4' y='50' font-size='9' "
      "fill='#666'>Momentum (M)</text></svg>";
  CHECK(svg == expected);
}

TEST_CASE("gauge clamps out-of-range values and picks the correct color band", "[charts]") {
  // python3: gauge(20) -> frac=0.2 < 0.34 -> red; gauge(50) -> frac=0.5, in
  // [0.34,0.67) -> amber; gauge(150,hi=100) clamps to 100 -> frac=1.0 -> green.
  CHECK(gauge(20).find("fill='#c0392b'") != std::string::npos);
  CHECK(gauge(50).find("fill='#d68910'") != std::string::npos);
  std::string clamped = gauge(150, 0, 100);
  CHECK(clamped.find(">100<") != std::string::npos);  // clamped to hi=100
  CHECK(clamped.find("fill='#1e8449'") != std::string::npos);
}
