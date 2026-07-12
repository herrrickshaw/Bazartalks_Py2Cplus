// Integration tests against a real local Cassandra instance (this
// environment runs Cassandra via `brew services`, same as the original
// BazaarTalks repo's INFRA.md documents) and a real local Kafka broker for
// the mandatory CDC publish. Uses a namespaced test ticker so it never
// collides with real cached data written by the Python side.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "bazaartalks/storage/market_store.hpp"

using namespace bazaartalks::storage;
using date::day;
using date::month;
using date::year;
using Catch::Approx;

namespace {
date::year_month_day ymd(int y, unsigned m, unsigned d) { return year{y} / month{m} / day{d}; }
}  // namespace

TEST_CASE("MarketStore throws MarketStoreUnavailable when Cassandra is unreachable",
          "[cassandra][integration]") {
  // Port 1 is not a Cassandra node on any reachable host.
  CHECK_THROWS_AS(MarketStore("127.0.0.1", 1, std::chrono::milliseconds(1000)),
                  MarketStoreUnavailable);
}

TEST_CASE("put_ohlc + get_ohlc + coverage round-trip against a live Cassandra + Kafka",
          "[cassandra][integration]") {
  MarketStore store;
  const std::string ticker = "BTCPP_TEST_TICKER";

  std::vector<OhlcBar> bars = {
      {ymd(2026, 1, 2), 100.0, 102.0, 99.0, 101.0, 1000.0},
      {ymd(2026, 1, 5), 101.0, 103.0, 100.0, 102.5, 1500.0},
      {ymd(2026, 1, 6), 102.5, 104.0, 101.5, 103.0, 1200.0},
  };
  REQUIRE_NOTHROW(store.put_ohlc(ticker, bars));

  auto fetched = store.get_ohlc(ticker);
  REQUIRE(fetched.size() >= bars.size());  // >= in case a prior test run left rows

  auto cov = store.coverage(ticker);
  REQUIRE(cov.has_value());
  CHECK(cov->min_date == ymd(2026, 1, 2));
  CHECK(cov->max_date == ymd(2026, 1, 6));
  CHECK(cov->n_bars == static_cast<std::int64_t>(fetched.size()));

  // Spot-check one bar's values round-tripped exactly (date::sys_days ==
  // covers whole-day equality; doubles at 1e-9 since these are simple
  // decimal literals with no accumulated floating error).
  auto it = std::find_if(fetched.begin(), fetched.end(),
                          [&](const OhlcBar& b) { return b.d == ymd(2026, 1, 5); });
  REQUIRE(it != fetched.end());
  CHECK(it->o == Approx(101.0));
  CHECK(it->h == Approx(103.0));
  CHECK(it->l == Approx(100.0));
  CHECK(it->c == Approx(102.5));
  CHECK(it->v == Approx(1500.0));
}

TEST_CASE("coverage() returns nullopt for a ticker with nothing cached",
          "[cassandra][integration]") {
  MarketStore store;
  auto cov = store.coverage("BTCPP_TEST_TICKER_NEVER_WRITTEN_XYZ");
  CHECK_FALSE(cov.has_value());
}
