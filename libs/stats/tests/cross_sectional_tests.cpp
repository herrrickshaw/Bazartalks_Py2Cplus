// Golden tests generated from marketdata.py's zscore/information_coefficient/
// monotonicity/trend_corr, run against the actual Python module.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

#include "bazaartalks/stats/cross_sectional.hpp"

using bazaartalks::stats::information_coefficient;
using bazaartalks::stats::monotonicity;
using bazaartalks::stats::trend_corr;
using bazaartalks::stats::zscore;
using Catch::Approx;

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
}

// python3 -c "import marketdata as md, pandas as pd
// print(list(md.zscore(pd.Series([1.,2.,3.,4.,5.]))))"
// -> [-1.414213562373095, -0.7071067811865475, 0.0, 0.7071067811865475, 1.414213562373095]
TEST_CASE("zscore matches Python on a clean series", "[stats]") {
  auto z = zscore({1.0, 2.0, 3.0, 4.0, 5.0});
  REQUIRE(z.size() == 5);
  CHECK(z[0] == Approx(-1.414213562373095));
  CHECK(z[1] == Approx(-0.7071067811865475));
  CHECK(z[2] == Approx(0.0));
  CHECK(z[3] == Approx(0.7071067811865475));
  CHECK(z[4] == Approx(1.414213562373095));
}

// Degenerate cross-section (zero variance): Python collapses the WHOLE
// output to 0.0 -- pd.Series(0.0, index=s.index), not NaN-preserving.
TEST_CASE("zscore collapses degenerate cross-section to all-zero like Python", "[stats]") {
  auto z = zscore({5.0, 5.0, 5.0});
  CHECK(z == std::vector<double>{0.0, 0.0, 0.0});
}

// python3: md.zscore(pd.Series([1.,nan,3.,inf,5.]))
// -> [-1.224744871391589, nan, 0.0, nan, 1.224744871391589]
TEST_CASE("zscore treats +-inf as NaN and skips NaN from mean/std like Python", "[stats]") {
  auto z = zscore({1.0, kNaN, 3.0, std::numeric_limits<double>::infinity(), 5.0});
  REQUIRE(z.size() == 5);
  CHECK(z[0] == Approx(-1.224744871391589));
  CHECK(std::isnan(z[1]));
  CHECK(z[2] == Approx(0.0));
  CHECK(std::isnan(z[3]));
  CHECK(z[4] == Approx(1.224744871391589));
}

// python3: md.information_coefficient(sig, fwd) on a perfectly-correlated
// (monotone) signal/forward-return pair -> 1.0
TEST_CASE("information_coefficient matches Python on a monotone signal", "[stats]") {
  std::vector<double> sig = {0.1, 0.2, 0.15, 0.4, 0.05, 0.3, 0.25, 0.12, 0.22, 0.18, 0.35};
  std::vector<double> fwd = {0.01, 0.02, 0.015, 0.04, 0.005, 0.03, 0.025, 0.012, 0.022, 0.018, 0.035};
  CHECK(information_coefficient(sig, fwd) == Approx(1.0));
}

TEST_CASE("information_coefficient returns NaN below the 10-pair minimum", "[stats]") {
  std::vector<double> sig = {0.1, 0.2, 0.3};
  std::vector<double> fwd = {0.01, 0.02, 0.03};
  CHECK(std::isnan(information_coefficient(sig, fwd)));
}

// python3: md.monotonicity(pd.DataFrame({'q':[0.01,0.02,0.015,0.04,0.05]}), 'q') -> 0.8999999999999998
TEST_CASE("monotonicity matches Python on a mostly-rising quantile curve", "[stats]") {
  CHECK(monotonicity({0.01, 0.02, 0.015, 0.04, 0.05}) == Approx(0.8999999999999998));
}

TEST_CASE("trend_corr matches Python on rising/falling/noisy series", "[stats]") {
  CHECK(trend_corr({1, 2, 3, 4, 5}) == Approx(0.9999999999999999));
  CHECK(trend_corr({5, 4, 3, 2, 1}) == Approx(-0.9999999999999999));
  CHECK(trend_corr({1, 3, 2, 4, 6, 5, 7}) == Approx(0.9285714285714288));
}
