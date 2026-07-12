// This module is the Phase 6 correctness-critical gate, so its tests go
// beyond golden-diff: an exact-match test on which tickers are included
// in a given rebalance period (not just their values), and a dedicated
// lookahead-injection test proving a bar dated after the decision date
// cannot affect that decision. Golden values for monthly_rebalance_dates/
// period_returns cross-checked directly against pandas'
// `resample("MS").first()` + `idx.get_indexer(method="ffill")` on the same
// two-ticker fixture (see the commit message / PR description for the
// cross-check script); compute_metrics cross-checked against numpy.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "bazaartalks/quant_core/pit_backtest.hpp"

using namespace bazaartalks::quant_core;
using Catch::Approx;

namespace {

date::sys_days d(int y, unsigned m, unsigned dd) {
  return date::sys_days{date::year{y} / m / dd};
}

// AAA: daily closes 2024-01-01..2024-04-10, close = 100 + day-offset.
// BBB: daily closes 2024-02-15..2024-04-10 (a later listing), close = 50 + day-offset.
std::vector<TickerHistory> make_fixture() {
  TickerHistory aaa;
  aaa.ticker = "AAA";
  date::sys_days start_a = d(2024, 1, 1);
  date::sys_days end_a = d(2024, 4, 10);
  int i = 0;
  for (date::sys_days t = start_a; t <= end_a; t += date::days{1}, ++i) {
    aaa.dates.push_back(t);
    aaa.close.push_back(100.0 + static_cast<double>(i));
  }

  TickerHistory bbb;
  bbb.ticker = "BBB";
  date::sys_days start_b = d(2024, 2, 15);
  date::sys_days end_b = d(2024, 4, 10);
  int j = 0;
  for (date::sys_days t = start_b; t <= end_b; t += date::days{1}, ++j) {
    bbb.dates.push_back(t);
    bbb.close.push_back(50.0 + static_cast<double>(j));
  }

  return {aaa, bbb};
}

}  // namespace

TEST_CASE("darvas_breakout matches the prior-N-close new-high check", "[pit_backtest]") {
  std::vector<double> close = {10.0, 20.0, 15.0, 25.0, 30.0};
  CHECK(darvas_breakout(close, 4, /*lookback=*/4) == true);   // 30 >= max(10,20,15,25)
  CHECK(darvas_breakout(close, 2, /*lookback=*/2) == false);  // 15 < max(10,20)
  CHECK(darvas_breakout(close, 1, /*lookback=*/2) == false);  // loc < lookback
}

TEST_CASE("darvas_breakout skips NaN in the prior window like pandas .max()",
          "[pit_backtest]") {
  std::vector<double> close = {std::nan(""), std::nan(""), 20.0, 100.0};
  CHECK(darvas_breakout(close, 3, /*lookback=*/3) == true);  // 100 >= max(NaN,NaN,20)=20
}

TEST_CASE("compute_metrics matches numpy's population-std Sharpe and drawdown",
          "[pit_backtest]") {
  auto m = compute_metrics({5.0, -2.0, 3.0, 1.5, -0.5});
  CHECK(m.n_months == 5);
  CHECK(m.avg_month_pct == Approx(1.4));
  CHECK(m.ann_return_pct == Approx(17.73));
  CHECK(m.hit_rate_pct == Approx(60.0));
  CHECK(m.sharpe == Approx(1.96));
  CHECK(m.max_dd_pct == Approx(-2.0));
}

TEST_CASE("compute_metrics returns n_months=0 for an empty return series", "[pit_backtest]") {
  auto m = compute_metrics({});
  CHECK(m.n_months == 0);
}

TEST_CASE("monthly_rebalance_dates matches pandas resample('MS').first() + cutoff filter",
          "[pit_backtest]") {
  auto tickers = make_fixture();
  auto rebal = monthly_rebalance_dates(tickers, /*min_lookback=*/20);
  REQUIRE(rebal.size() == 3);
  CHECK(rebal[0] == d(2024, 2, 1));
  CHECK(rebal[1] == d(2024, 3, 1));
  CHECK(rebal[2] == d(2024, 4, 1));
}

TEST_CASE("period_returns gives the exact ticker-inclusion set and forward returns "
          "for period Feb1->Mar1 (BBB not yet listed)",
          "[pit_backtest]") {
  auto tickers = make_fixture();
  auto rows = period_returns(tickers, d(2024, 2, 1), d(2024, 3, 1), /*darvas_lookback=*/20);
  // Exact-match inclusion set: only AAA, BBB is absent (no BBB date <= Feb 1).
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].ticker == "AAA");
  CHECK(rows[0].forward_return_pct == Approx(22.137404580152676).epsilon(1e-9));
  CHECK(rows[0].is_darvas_breakout == true);
}

TEST_CASE("period_returns gives the exact ticker-inclusion set and forward returns "
          "for period Mar1->Apr1 (BBB now listed but below the breakout lookback)",
          "[pit_backtest]") {
  auto tickers = make_fixture();
  auto rows = period_returns(tickers, d(2024, 3, 1), d(2024, 4, 1), /*darvas_lookback=*/20);
  REQUIRE(rows.size() == 2);
  auto find = [&](const std::string& t) -> const PeriodReturn& {
    for (const auto& r : rows) if (r.ticker == t) return r;
    throw std::runtime_error("not found");
  };
  const auto& aaa = find("AAA");
  CHECK(aaa.forward_return_pct == Approx(19.375000).epsilon(1e-9));
  CHECK(aaa.is_darvas_breakout == true);

  const auto& bbb = find("BBB");
  CHECK(bbb.forward_return_pct == Approx(47.692307692).epsilon(1e-9));
  // BBB's entry bar (Mar 1) is only 15 trading days into its listing --
  // below the 20-bar lookback -- so this must be False purely from the
  // `loc < lookback` guard, not a real proximity check.
  CHECK(bbb.is_darvas_breakout == false);
}

TEST_CASE("period_returns is immune to a bar dated after the decision date (lookahead "
          "injection)",
          "[pit_backtest]") {
  auto baseline = make_fixture();
  auto baseline_rows =
      period_returns(baseline, d(2024, 2, 1), d(2024, 3, 1), /*darvas_lookback=*/20);

  // Inject an extra AAA bar dated ONE DAY AFTER the Feb-1 decision date,
  // with an absurd close value that would corrupt the entry snap or the
  // breakout's "prior max" if the engine ever looked forward from t0.
  auto injected = baseline;
  // injected[0] is AAA (see make_fixture); insert the extra bar right
  // after its Feb-1 entry (index 31: Jan has 31 days, so Feb-1 is index 31).
  std::size_t feb1_idx = 31;
  REQUIRE(injected[0].dates[feb1_idx] == d(2024, 2, 1));
  injected[0].dates.insert(injected[0].dates.begin() + feb1_idx + 1, d(2024, 2, 2));
  injected[0].close.insert(injected[0].close.begin() + feb1_idx + 1, 999999.0);

  auto injected_rows =
      period_returns(injected, d(2024, 2, 1), d(2024, 3, 1), /*darvas_lookback=*/20);

  REQUIRE(injected_rows.size() == baseline_rows.size());
  for (std::size_t i = 0; i < baseline_rows.size(); ++i) {
    CHECK(injected_rows[i].ticker == baseline_rows[i].ticker);
    CHECK(injected_rows[i].forward_return_pct ==
          Approx(baseline_rows[i].forward_return_pct).epsilon(1e-12));
    CHECK(injected_rows[i].is_darvas_breakout == baseline_rows[i].is_darvas_breakout);
  }
}
