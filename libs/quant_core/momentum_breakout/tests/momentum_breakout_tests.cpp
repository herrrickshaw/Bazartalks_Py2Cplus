// This module implements a NEW strategy (arXiv:2412.14361v1's Keltner-
// Donchian breakout), not a port of an existing Python original -- there
// is no prior codebase output to match. Tests instead cross-check the
// formulas against an independent pandas/numpy implementation of the same
// published equations (see each TEST_CASE's comment for the reference
// script), and hand-computed values for the simpler scalar functions.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "bazaartalks/quant_core/momentum_breakout.hpp"

using namespace bazaartalks::quant_core;
using Catch::Approx;

namespace {
const std::vector<double> kClose = {100.0, 101.0, 99.5, 102.0, 103.5, 101.0,
                                     104.0, 106.0, 105.0, 107.5, 108.0, 106.5};
}

// Cross-check via an independent pandas/numpy implementation of the same
// paper equations (Keltner: EMA(close,n)+/-1.4*k*rolling_mean(|diff|,n);
// Donchian: rolling max/min INCLUDING the current bar; combined
// upper=min(Donchian,Keltner), lower=max(Donchian,Keltner)) on this
// fixed 12-bar series, n=5, k=1.0:
//   bar 4:  upper=103.5    lower=99.5
//   bar 6:  upper=104.0    lower=99.5
//   bar 7:  upper=106.0    lower=101.0
//   bar 8:  upper=106.0    lower=101.321694
//   bar 11: upper=108.0    lower=105.0
TEST_CASE("combined_channel_band matches an independent pandas/numpy cross-check",
          "[momentum_breakout]") {
  auto bands = combined_channel_band(kClose, 5, 1.0);
  REQUIRE(bands.size() == kClose.size());

  for (std::size_t i = 0; i < 4; ++i) {
    CHECK(std::isnan(bands[i].upper));
    CHECK(std::isnan(bands[i].lower));
  }
  CHECK(bands[4].upper == Approx(103.5));
  CHECK(bands[4].lower == Approx(99.5));
  CHECK(bands[6].upper == Approx(104.0));
  CHECK(bands[6].lower == Approx(99.5));
  CHECK(bands[7].upper == Approx(106.0));
  CHECK(bands[7].lower == Approx(101.0));
  CHECK(bands[8].upper == Approx(106.0));
  CHECK(bands[8].lower == Approx(101.321694).margin(1e-5));
  CHECK(bands[11].upper == Approx(108.0));
  CHECK(bands[11].lower == Approx(105.0));
}

TEST_CASE("update_trailing_stop only ever ratchets up, never down", "[momentum_breakout]") {
  double stop = 95.0;
  stop = update_trailing_stop(stop, 97.0);  // lower band rose -> stop follows
  CHECK(stop == Approx(97.0));
  stop = update_trailing_stop(stop, 93.0);  // lower band fell -> stop does NOT retreat
  CHECK(stop == Approx(97.0));
  stop = update_trailing_stop(stop, 99.0);  // rose again -> stop follows
  CHECK(stop == Approx(99.0));
}

TEST_CASE("breakout_entry_signal and breakout_exit_signal match the paper's threshold rules",
          "[momentum_breakout]") {
  CHECK(breakout_entry_signal(105.0, 104.9) == true);   // price >= prev upper band -> enter
  CHECK(breakout_entry_signal(104.0, 104.9) == false);  // below the band -> no entry
  CHECK(breakout_entry_signal(104.9, 104.9) == true);   // exactly at the band -> enter (>=)

  CHECK(breakout_exit_signal(99.0, 100.0) == true);    // price <= trailing stop -> exit
  CHECK(breakout_exit_signal(101.0, 100.0) == false);  // still above the stop -> hold
  CHECK(breakout_exit_signal(100.0, 100.0) == true);   // exactly at the stop -> exit (<=)
}

// hand-computed: w = target_vol/vol per asset, no rescale needed since
// sum stays under max_leverage.
TEST_CASE("inverse_vol_weights sizes inversely to volatility with no rescale needed",
          "[momentum_breakout]") {
  std::vector<double> vols = {0.1, 0.2, 0.05};
  auto w = inverse_vol_weights(vols, 0.02, 2.0);
  CHECK(w[0] == Approx(0.2));
  CHECK(w[1] == Approx(0.1));
  CHECK(w[2] == Approx(0.4));
}

// hand-computed: raw weights = [2.0, 1.0, 2.0], sum=5.0 > max_leverage=2.0
// -> rescale factor = 2.0/5.0 = 0.4 -> [0.8, 0.4, 0.8], sum exactly 2.0.
TEST_CASE("inverse_vol_weights rescales proportionally when gross exposure exceeds max_leverage",
          "[momentum_breakout]") {
  std::vector<double> vols = {0.01, 0.02, 0.01};
  auto w = inverse_vol_weights(vols, 0.02, 2.0);
  CHECK(w[0] == Approx(0.8));
  CHECK(w[1] == Approx(0.4));
  CHECK(w[2] == Approx(0.8));
  double sum = w[0] + w[1] + w[2];
  CHECK(sum == Approx(2.0));
}

TEST_CASE("inverse_vol_weights gives zero weight to a zero-volatility asset",
          "[momentum_breakout]") {
  std::vector<double> vols = {0.1, 0.0};
  auto w = inverse_vol_weights(vols, 0.02, 2.0);
  CHECK(w[1] == Approx(0.0));
}
