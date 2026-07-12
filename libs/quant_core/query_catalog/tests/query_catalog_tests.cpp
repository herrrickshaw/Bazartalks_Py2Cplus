// SECURITY-CRITICAL adversarial test suite for validate_predicate(),
// per the migration plan's Phase 8 gate: "build a dedicated adversarial
// test suite (stacked queries, comment sequences, UNION/boolean-blind
// patterns) run against both the Python and C++ validators, asserting
// identical accept/reject decisions". Every case below (accept/reject
// decision AND exact rejection message where noted) was verified against
// the actual serve.py validate_predicate() by direct execution -- see the
// commit message / PR description for the verification script.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/quant_core/query_catalog.hpp"

using namespace bazaartalks::quant_core;

namespace {
bool accepts(const std::string& pred) {
  try {
    validate_predicate(pred);
    return true;
  } catch (const QueryCatalogError&) {
    return false;
  }
}

std::string rejection_message(const std::string& pred) {
  try {
    validate_predicate(pred);
    return "<accepted>";
  } catch (const QueryCatalogError& e) {
    return e.what();
  }
}
}  // namespace

TEST_CASE("build_query returns the exact catalog SQL, substituting {limit} only where present",
          "[query_catalog]") {
  CHECK(build_query("markets") ==
        "SELECT market, count(DISTINCT ticker) tickers, count(*) bars "
        "FROM ohlc GROUP BY 1 ORDER BY tickers DESC");
  // "markets" has no {limit} placeholder -- limit is silently unused,
  // matching Python's `.format(limit=...)` on a string without that key.
  CHECK(build_query("markets", 999) == build_query("markets", 1));

  CHECK(build_query("ggg", 10) ==
        "SELECT market, ticker, D, V, M, composite, label FROM dvm_composite "
        "WHERE code='GGG' ORDER BY composite DESC LIMIT 10");
  CHECK(build_query("dvm_dist") ==
        "SELECT code, label, count(*) n FROM dvm_composite GROUP BY 1,2 ORDER BY n DESC");
  CHECK(build_query("high_roe_low_de", 5) ==
        "SELECT market, ticker, roe, de, pe, sector FROM fundamentals "
        "WHERE roe>15 AND de<1 AND de IS NOT NULL ORDER BY roe DESC LIMIT 5");
}

TEST_CASE("build_query throws for an unknown query name", "[query_catalog]") {
  CHECK_THROWS_AS(build_query("nonexistent"), QueryCatalogError);
}

TEST_CASE("validate_predicate rejects stacked queries (semicolon anywhere)",
          "[query_catalog][security]") {
  CHECK(accepts("roe>15; DROP TABLE fundamentals;--") == false);
  CHECK(accepts("roe>15;SELECT*FROM users") == false);
  CHECK(accepts("roe>15;") == false);
  CHECK(rejection_message("roe>15;") == "illegal characters in predicate");
}

TEST_CASE("validate_predicate rejects comment sequences (-- and /*)",
          "[query_catalog][security]") {
  CHECK(accepts("roe>15 -- ' OR '1'='1") == false);
  CHECK(accepts("roe>15/**/OR/**/1=1") == false);
  CHECK(accepts("roe>15 /*comment*/") == false);
  CHECK(rejection_message("roe>15 /*comment*/") == "illegal characters in predicate");
}

TEST_CASE("validate_predicate rejects UNION-based injection via the identifier allow-list",
          "[query_catalog][security]") {
  CHECK(accepts("roe>15 UNION SELECT password FROM users") == false);
  CHECK(rejection_message("roe>15 UNION SELECT password FROM users") ==
        "unknown identifiers in predicate: ['FROM', 'SELECT', 'UNION', 'password', 'users']");
  CHECK(accepts("roe>15 UNION ALL SELECT 1,2,3") == false);
  CHECK(rejection_message("roe>15 UNION ALL SELECT 1,2,3") ==
        "unknown identifiers in predicate: ['ALL', 'SELECT', 'UNION']");
}

TEST_CASE("validate_predicate rejects subselect/function-call injection attempts",
          "[query_catalog][security]") {
  CHECK(accepts("roe>(SELECT count(*) FROM fundamentals)") == false);
  CHECK(rejection_message("roe>(SELECT count(*) FROM fundamentals)") ==
        "unknown identifiers in predicate: ['FROM', 'SELECT', 'count', 'fundamentals']");
  CHECK(accepts("roe>15 AND (SELECT 1)=1") == false);
}

TEST_CASE("validate_predicate rejects quote-based injection attempts",
          "[query_catalog][security]") {
  CHECK(accepts("' OR '1'='1") == false);
  CHECK(rejection_message("' OR '1'='1") == "predicate shape not allowed");
  CHECK(accepts("roe>15 OR de='x'") == false);
  CHECK(accepts("de='1' OR de='1'") == false);
}

TEST_CASE("validate_predicate ACCEPTS the boolean-blind tautology 1=1 -- a known, "
          "deliberately-unfixed weakness of the Python original, replicated verbatim",
          "[query_catalog][security]") {
  CHECK(accepts("roe>15 OR 1=1") == true);
  CHECK(accepts("1=1") == true);
  CHECK(accepts("roe>0 OR roe<0 OR 1=1") == true);
}

TEST_CASE("validate_predicate's and/or keyword matching is case-sensitive to exactly "
          "and/or/AND/OR",
          "[query_catalog][security]") {
  CHECK(accepts("roe>15 and de<1") == true);
  CHECK(accepts("roe>15 AND de<1") == true);
  CHECK(accepts("roe>15 or de<1") == true);
  CHECK(accepts("roe>15 OR de<1") == true);
  // Any other casing is an unrecognized identifier, not a keyword.
  CHECK(accepts("roe>15 And de<1") == false);
  CHECK(rejection_message("roe>15 And de<1") == "unknown identifiers in predicate: ['And']");
  CHECK(accepts("roe>15 aNd de<1") == false);
  CHECK(accepts("roe>15 Or de<1") == false);
}

TEST_CASE("validate_predicate's column allow-list check is case-sensitive",
          "[query_catalog][security]") {
  CHECK(accepts("roe>15") == true);
  CHECK(accepts("ROE>15") == false);  // allow-list has "roe", not "ROE"
  CHECK(rejection_message("ROE>15") == "unknown identifiers in predicate: ['ROE']");
  CHECK(accepts("Roe>15") == false);
}

TEST_CASE("validate_predicate accepts every legitimate predicate shape", "[query_catalog]") {
  CHECK(accepts("roe>15") == true);
  CHECK(accepts("roe>15 and de<1") == true);
  CHECK(accepts("roe > 15 AND de < 1.5") == true);
  CHECK(accepts("(roe>15 and de<1) or pe<10") == true);
  CHECK(accepts("composite>=70") == true);
  CHECK(accepts("mktcap>1000000000") == true);
  CHECK(accepts("div_yield>0.02") == true);
  CHECK(accepts("roe>15\tAND de<1") == true);   // tab
  CHECK(accepts("roe>15\nAND de<1") == true);   // newline
  CHECK(accepts("roe>15\rAND de<1") == true);   // carriage return
}

TEST_CASE("validate_predicate edge cases: empty, whitespace-only", "[query_catalog]") {
  CHECK(accepts("") == false);
  CHECK(rejection_message("") == "predicate shape not allowed");
  // A whitespace-only predicate genuinely IS accepted by the Python
  // original (matches `[\w\s...]+`, no identifiers to reject) -- an odd
  // but real behavior, replicated as-is.
  CHECK(accepts("   ") == true);
}

TEST_CASE("validate_predicate's stricter (ASCII-only) \\w can only reject MORE than "
          "Python, never less -- the safe direction for a security gate",
          "[query_catalog][security]") {
  // Python ACCEPTS this: a lone Unicode letter is never even extracted as
  // an "identifier" (the findall pattern's first char class [A-Za-z_] is
  // ASCII-only in BOTH engines, so `é` alone never enters the bad-
  // identifier check), and Python's Unicode-aware `\w` in the shape regex
  // matches it, so the whole-string shape check passes. This port's
  // ASCII-only std::regex `\w` does NOT match `é` in the shape check, so
  // it rejects here where Python would accept -- over-rejection, not a
  // bypass, and exactly the divergence direction the header comment
  // argues is safe.
  CHECK(accepts("roe>15 and \xc3\xa9>1") == false);  // "roe>15 and é>1" (UTF-8)

  // Python REJECTS this too (identifier check catches "roeé" as one
  // token via its own Unicode \w), but by a DIFFERENT gate than this
  // port (which stops identifier-extraction at the non-ASCII byte,
  // extracts "roe" -- allow-listed -- then rejects at the shape check
  // instead). Same outcome (reject), different internal reason; the
  // outcome is what the /filter endpoint's callers actually depend on.
  CHECK(accepts("roe\xc3\xa9>15") == false);  // "roeé>15" (UTF-8)
}
