#pragma once
// Port of serve.py's pure query-builder/validator -- the part of the
// serving layer the Python original's own docstring calls out as
// importable with no heavy deps, so it can be unit-tested in isolation.
// This is the SECURITY-CRITICAL gate for Phase 8: `validate_predicate()`
// is the only thing standing between the internet-facing `/filter`
// endpoint and arbitrary SQL, so it is ported and tested with unusual
// care -- see validate_predicate()'s own comment for the specific
// Python-`re`-vs-`std::regex` divergence considered and why it's safe.
//
// `_connect()`/`create_app()`'s FastAPI routes, DuckDB wiring, and the
// watchlist CRUD (over a separate `watchlist_store.py`/vcrud subsystem)
// are the actual HTTP serving layer -- see services/serve_api for the
// Drogon port of the read-only routes this module backs. Watchlist CRUD
// is explicitly deferred (not silently dropped): it requires porting
// watchlist_store.py's own versioned-CRUD schema/semantics, a substantial
// side quest orthogonal to this phase's actual gate (the read-only
// `/filter` predicate validator).

#include <stdexcept>
#include <string>

namespace bazaartalks::quant_core {

// Raised for both build_query()'s "unknown query name" (Python KeyError)
// and validate_predicate()'s three rejection reasons (Python ValueError)
// -- callers distinguish which by inspecting what(), matching how the
// Python original's HTTPException(404)/HTTPException(400) call sites
// already branch on exception type, not message content.
class QueryCatalogError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Port of build_query(): returns the named catalog query's SQL, with
// `{limit}` substituted for `limit` if the query actually has that
// placeholder (matching Python's `.format(limit=...)`, which silently
// ignores an unused keyword argument -- "markets" and "dvm_dist" have no
// `{limit}` placeholder at all, so `limit` is simply unused for those).
// Throws QueryCatalogError for an unknown `name`.
std::string build_query(const std::string& name, int limit = 25);

// Port of validate_predicate(): a SQL-injection allow-list guard for an
// ad-hoc `/filter` predicate, applied in this exact order (matching the
// Python original's own gate ordering, which matters -- e.g. a predicate
// with both an illegal substring AND a bad identifier reports the
// illegal-substring rejection first, since that check runs first):
//   1. Reject if the raw string contains ';', '--', or '/*' anywhere.
//   2. Extract every `[A-Za-z_][A-Za-z0-9_]*`-shaped token (case-
//      sensitive) and reject if any isn't in the column allow-list or
//      exactly "and"/"or"/"AND"/"OR" (note: NOT case-insensitive beyond
//      those 4 literal spellings -- "And"/"AND " with trailing space/
//      "OR"+lowercase-mixed like "Or" are all rejected as unknown
//      identifiers, and a column name in the wrong case, e.g. "ROE" vs
//      the allow-listed "roe", is rejected too).
//   3. Reject if the whole string doesn't match the allowed shape
//      (letters/digits/underscore/whitespace/`.`/`>`/`<`/`=`/`!`/`(`/`)`
//      only, case-insensitively for the regex itself though not for the
//      identifier check in step 2).
// Returns `pred` unchanged (this validates, it does not sanitize/rewrite)
// on success. Throws QueryCatalogError with one of the three exact
// English messages the Python `ValueError`s carry (the identifiers list
// in message 2 is sorted, matching Python's `sorted(bad)`).
//
// KNOWN, DELIBERATELY-UNFIXED WEAKNESS (replicated, not patched): a
// predicate like "1=1" or "roe>15 or 1=1" is ACCEPTED -- it has no
// disallowed identifiers (no letters at all in "1=1") and matches the
// shape regex. This is a classic SQL boolean-blind pattern, but note
// there is no way to STACK an additional statement or exfiltrate data
// through it here (no semicolon, no UNION, no comment -- those are
// caught by steps 1/2) -- it can only make the WHERE clause more
// permissive within the single SELECT the caller already controls the
// shape of. "Fixing" this would silently change the trust boundary this
// function has actually enforced in production; that decision belongs to
// the application, not a transliteration of it.
//
// Python-`re`-vs-`std::regex` divergence considered: Python's `\w` is
// Unicode-aware by default (matches any Unicode letter/digit/underscore),
// while C++'s ECMAScript-grammar `\w` (used here) is ASCII-only. Since
// ASCII word characters are a strict SUBSET of Python's Unicode word
// characters, this port's identifier-shape checks can only be AT LEAST as
// strict as the Python original -- a predicate containing a non-ASCII
// Unicode letter can only be rejected here where Python might have
// accepted it, never the reverse. That is the safe direction for a
// security gate (over-rejection, not a new bypass), and is covered by a
// dedicated test rather than left as an unexamined assumption.
std::string validate_predicate(const std::string& pred);

}  // namespace bazaartalks::quant_core
