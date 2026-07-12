// Golden tests generated from market_holidays.py (run against the actual
// Python module — see the migration plan's validation strategy). These are
// exact-match checks: a date being in/out of a trading calendar is a
// boolean-inclusion property, not a tolerance question.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/calendar/trading_calendar.hpp"

using namespace bazaartalks::calendar;
using date::day;
using date::month;
using date::year;

namespace {
date::year_month_day ymd(int y, unsigned m, unsigned d) {
  return year{y} / month{m} / day{d};
}
}  // namespace

TEST_CASE("market alias resolution matches Python's ALIASES/_canon", "[calendar]") {
  CHECK(market_from_alias("NSE") == Market::India);
  CHECK(market_from_alias("BSE") == Market::India);
  CHECK(market_from_alias("NYSE") == Market::US);
  CHECK(market_from_alias("NASDAQ") == Market::US);
  CHECK(market_from_alias("TSE") == Market::Japan);
  CHECK(market_from_alias("JPX") == Market::Japan);
  CHECK(market_from_alias("KRX") == Market::Korea);
  CHECK(market_from_alias("KOSPI") == Market::Korea);
  CHECK(market_from_alias("EU") == Market::Europe);
  CHECK(market_from_alias("XETRA") == Market::Europe);
  CHECK_THROWS_AS(market_from_alias("MARS"), std::invalid_argument);
}

// python3 -c "import market_holidays as mh; print(mh.is_trading_day('US','2026-01-19'))" etc.
TEST_CASE("is_trading_day matches Python golden spot checks", "[calendar]") {
  CHECK_FALSE(is_trading_day(Market::US, ymd(2026, 1, 1)));    // New Year
  CHECK_FALSE(is_trading_day(Market::US, ymd(2026, 1, 19)));   // MLK (curated)
  CHECK_FALSE(is_trading_day(Market::US, ymd(2026, 7, 4)));    // July 4th (fixed)
  CHECK(is_trading_day(Market::US, ymd(2026, 7, 3)));          // ordinary Friday
  CHECK_FALSE(is_trading_day(Market::US, ymd(2026, 11, 26)));  // Thanksgiving (curated)

  CHECK_FALSE(is_trading_day(Market::India, ymd(2026, 1, 26)));   // Republic Day
  CHECK_FALSE(is_trading_day(Market::India, ymd(2026, 3, 4)));    // Holi (curated)
  CHECK_FALSE(is_trading_day(Market::India, ymd(2026, 8, 15)));   // Independence Day
  CHECK_FALSE(is_trading_day(Market::India, ymd(2026, 11, 9)));   // Diwali (curated)
  CHECK(is_trading_day(Market::India, ymd(2026, 11, 10)));        // ordinary Tuesday

  CHECK_FALSE(is_trading_day(Market::Japan, ymd(2026, 5, 4)));  // Golden Week (fixed)
  CHECK(is_trading_day(Market::Japan, ymd(2026, 5, 6)));        // ordinary Wednesday

  CHECK_FALSE(is_trading_day(Market::Korea, ymd(2026, 9, 24)));  // Chuseok (curated)
  CHECK(is_trading_day(Market::Korea, ymd(2026, 9, 23)));         // ordinary Wednesday

  CHECK_FALSE(is_trading_day(Market::Europe, ymd(2026, 1, 1)));
  CHECK_FALSE(is_trading_day(Market::Europe, ymd(2026, 12, 25)));
}

// python3 -c "import market_holidays as mh; print(len(mh.trading_days('US','2026-01-01','2026-12-31')))"
TEST_CASE("trading_days full-year 2026 counts match Python exactly", "[calendar]") {
  auto count = [](Market m) {
    return trading_days(m, ymd(2026, 1, 1), ymd(2026, 12, 31)).size();
  };
  CHECK(count(Market::US) == 253);
  CHECK(count(Market::India) == 253);
  CHECK(count(Market::Japan) == 245);
  CHECK(count(Market::Korea) == 251);
  CHECK(count(Market::Europe) == 255);
}

// python3 -c "import market_holidays as mh; print(mh.next_trading_day('US', date(2026,7,3)))"
TEST_CASE("next_trading_day skips weekends/holidays like Python", "[calendar]") {
  CHECK(next_trading_day(Market::US, ymd(2026, 7, 3)) == ymd(2026, 7, 6));
  // Europe: Easter 2026 = Apr 5 -> Good Friday Apr 3, Easter Monday Apr 6.
  CHECK(next_trading_day(Market::Europe, ymd(2026, 4, 2)) == ymd(2026, 4, 7));
}

TEST_CASE("easter() matches the known 2026 date", "[calendar]") {
  CHECK(easter(2026) == ymd(2026, 4, 5));
}

// pandas reference:
//   idx = DatetimeIndex(['2026-01-02','2026-01-05','2026-01-06','2026-01-09'])
//   idx.get_indexer(['2026-01-01','2026-01-05','2026-01-07','2026-01-10'], method='ffill')
//   -> [-1, 1, 2, 3]
TEST_CASE("ffill_index matches pandas get_indexer(method='ffill') exactly", "[calendar]") {
  std::vector<date::sys_days> idx = {
      date::sys_days{ymd(2026, 1, 2)},
      date::sys_days{ymd(2026, 1, 5)},
      date::sys_days{ymd(2026, 1, 6)},
      date::sys_days{ymd(2026, 1, 9)},
  };
  CHECK(ffill_index(idx, date::sys_days{ymd(2026, 1, 1)}) == std::nullopt);  // before all -> -1
  CHECK(ffill_index(idx, date::sys_days{ymd(2026, 1, 5)}) == 1);            // exact match
  CHECK(ffill_index(idx, date::sys_days{ymd(2026, 1, 7)}) == 2);            // nearest prior
  CHECK(ffill_index(idx, date::sys_days{ymd(2026, 1, 10)}) == 3);           // after all -> last
}
