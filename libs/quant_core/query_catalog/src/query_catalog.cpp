#include "bazaartalks/quant_core/query_catalog.hpp"

#include <algorithm>
#include <regex>
#include <set>
#include <unordered_map>
#include <vector>

namespace bazaartalks::quant_core {

namespace {

const std::unordered_map<std::string, std::string>& query_catalog() {
  static const std::unordered_map<std::string, std::string> kCatalog = {
      {"markets", "SELECT market, count(DISTINCT ticker) tickers, count(*) bars "
                  "FROM ohlc GROUP BY 1 ORDER BY tickers DESC"},
      {"ggg", "SELECT market, ticker, D, V, M, composite, label FROM dvm_composite "
              "WHERE code='GGG' ORDER BY composite DESC LIMIT {limit}"},
      {"dvm_dist", "SELECT code, label, count(*) n FROM dvm_composite GROUP BY 1,2 ORDER BY n DESC"},
      {"high_roe_low_de", "SELECT market, ticker, roe, de, pe, sector FROM fundamentals "
                          "WHERE roe>15 AND de<1 AND de IS NOT NULL ORDER BY roe DESC LIMIT {limit}"},
  };
  return kCatalog;
}

const std::set<std::string>& allowed_cols() {
  static const std::set<std::string> kCols = {"roe",  "de",         "pe",          "pb",
                                              "D",    "V",          "M",           "composite",
                                              "roa",  "rev_growth", "earn_growth", "op_margin",
                                              "div_yield", "mktcap"};
  return kCols;
}

}  // namespace

std::string build_query(const std::string& name, int limit) {
  const auto& catalog = query_catalog();
  auto it = catalog.find(name);
  if (it == catalog.end()) {
    // Python: f"unknown query {name!r}; choices={sorted(QUERY_CATALOG)}"
    std::vector<std::string> names;
    for (const auto& [k, v] : catalog) names.push_back(k);
    std::sort(names.begin(), names.end());
    std::string choices = "[";
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (i) choices += ", ";
      choices += "'" + names[i] + "'";
    }
    choices += "]";
    throw QueryCatalogError("unknown query '" + name + "'; choices=" + choices);
  }

  std::string sql = it->second;
  std::string placeholder = "{limit}";
  auto pos = sql.find(placeholder);
  if (pos != std::string::npos) {
    sql.replace(pos, placeholder.size(), std::to_string(limit));
  }
  return sql;
}

std::string validate_predicate(const std::string& pred) {
  // Gate 1: illegal substrings anywhere in the raw string.
  if (pred.find(';') != std::string::npos || pred.find("--") != std::string::npos ||
      pred.find("/*") != std::string::npos) {
    throw QueryCatalogError("illegal characters in predicate");
  }

  // Gate 2: every extracted identifier must be in the allow-list or
  // exactly one of and/or/AND/OR (case-sensitive -- see header comment).
  static const std::regex kIdentRe(R"([A-Za-z_][A-Za-z0-9_]*)");
  static const std::set<std::string> kKeywords = {"and", "or", "AND", "OR"};
  std::set<std::string> bad;
  for (auto it = std::sregex_iterator(pred.begin(), pred.end(), kIdentRe);
       it != std::sregex_iterator(); ++it) {
    std::string ident = it->str();
    if (!allowed_cols().count(ident) && !kKeywords.count(ident)) {
      bad.insert(ident);
    }
  }
  if (!bad.empty()) {
    std::string list = "[";
    bool first = true;
    for (const auto& b : bad) {  // std::set already sorted, matching sorted(bad)
      if (!first) list += ", ";
      list += "'" + b + "'";
      first = false;
    }
    list += "]";
    throw QueryCatalogError("unknown identifiers in predicate: " + list);
  }

  // Gate 3: whole-string shape check. ECMAScript's `\w` is ASCII-only,
  // a strict subset of Python's Unicode-aware `\w` -- see the header
  // comment for why that makes this check only ever MORE strict, never
  // more permissive, than the Python original.
  static const std::regex kShapeRe(R"(^[\w\s.><=!()]+(and|or|[\w\s.><=!()])*$)",
                                   std::regex::icase);
  if (!std::regex_match(pred, kShapeRe)) {
    throw QueryCatalogError("predicate shape not allowed");
  }

  return pred;
}

}  // namespace bazaartalks::quant_core
