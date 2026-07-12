# BazaarTalks: Python → C++ Migration Plan

## Context

`herrrickshaw/BazaarTalks` is a ~90-file, ~920KB multi-market quant research platform (DuckDB warehouse, Cassandra OHLC cache, Kafka/Flink streaming, FastAPI serving, pandas/sklearn factor research, live scraping integrations). The user wants its functionality redesigned in C++ rather than left in Python.

Two research passes (quant-core modules, infra/storage/serving modules) mapped every file's role, inputs/outputs, and port difficulty. The central finding: **this is not a uniform rewrite**. A meaningful chunk of the platform (yfinance, nsepython/NSEpy/bseindia/kabupy/pykrx scraping, Playwright-driven Trendlyne access) has **no C++ equivalent at all** and would be actively worse to reimplement (fragile, unofficial-API reverse-engineering) than to keep in Python. The rest — storage clients, streaming, factor/quant math, backtests, HTTP serving, and (per a follow-up GitHub search, see §ML below) the ML training stack — has mature native C++ paths and real performance upside.

**Update (post-research-verification):** the original plan asserted sklearn's `GradientBoostingClassifier`/`PCA`/`KMeans`/`IsolationForest` had "no closed-form C++ equivalent" and should stay in the Python sidecar. A GitHub search turned up real, actively-maintained, native C++ libraries for all four, so this was wrong and has been revised — see "ML Training: Revised Decision" below. The yfinance/NSE-live/Trendlyne data-acquisition boundary was re-checked the same way and holds up: no comparable native C++ replacement exists there.

The plan below commits to a **hybrid architecture**: a permanent, pared-down Python "acquisition + ML-training sidecar" for the parts with no C++ equivalent, feeding the exact same parquet/SQLite/Cassandra artifacts the platform already uses as its storage interface, while everything else becomes native C++. This mirrors a boundary the repo already draws for itself (`market_store.py`'s mandatory-Cassandra/no-silent-yfinance-fallback design; the repo's own documented preference for pure-Kafka consumers over PyFlink).

## Decision: The Python Boundary

**Permanent Python sidecar** (never ported): yfinance/nsepython/NSEpy/bseindia/kabupy/pykrx scanners and universe fetchers (except bhavcopy bulk EOD downloads, see below), `nse_data_fetcher.py`'s live/session-scraped calls, `trendlyne_session.py` (Playwright), `screener_session.py`, EDGAR/CVM/GLEIF-type scraping glue where cheap. Its only contract with the C++ side is writing to the **same** parquet layout, `fundamentals_cache.db`/`edgar_facts.db` SQLite schemas, and Cassandra `market.ohlc_bars` table already in use — no new IPC protocol.

**Native C++ core**: storage clients (DuckDB/Cassandra/Kafka/SQLite/Parquet), streaming consumers, factor/compute math, PIT/backtest engines, HTTP serving, **and now the full ML training stack** (see below — this was revised after verifying real library options exist).

**Refinement**: don't split strictly on "training vs inference" for ML — `ml_signal_engine.py`'s Ridge regression is closed-form linear algebra and moves to C++ directly via Eigen (no ONNX needed), and per the ML section below, `ml_screen_discovery.py`'s full estimator stack now has a native-C++ training path too, so there's no Python-trained-model-to-serve problem left to solve with ONNX at all.

**Bhavcopy nuance**: NSE's bulk end-of-day bhavcopy files are plain HTTP downloads (CSV/ZIP), not a scraped/session-authenticated API — portable to libcurl + a CSV parser like every other Phase-2/8 HTTP client. Only `nsepython`'s *live* calls (VIX, FII/DII flows, bulk/block deals, session-based quotes) are the genuinely non-portable, anti-bot-protected part of `nse_data_fetcher.py`.

This shrinks the real C++ port surface to roughly 55-60 of the ~90 files; the rest stay Python permanently by design, not as deferred work.

## ML Training: Revised Decision

The original plan (before this repo existed) asserted `ml_screen_discovery.py`'s sklearn stack — `StandardScaler`, `PCA`, `KMeans`, `GradientBoostingClassifier`, `IsolationForest` — had no closed-form C++ equivalent and should stay in the Python sidecar, with ONNX-exported inference as a maybe-later option. A GitHub search (with each candidate checked for real activity via `gh repo view`, not just search-result summaries) overturned that:

| sklearn estimator | Verified C++ replacement | Activity check |
|---|---|---|
| `PCA` | Not even a library dependency — PCA is closed-form eigendecomposition/SVD of the covariance matrix, already covered by Phase 1's `libs/linalg` (Eigen) | N/A |
| `KMeans` | [mlpack](https://github.com/mlpack/mlpack) — header-only C++ ML library, has KMeans built in | 5.7k★, pushed 2 days before this check |
| `GradientBoostingClassifier` | [XGBoost](https://github.com/dmlc/xgboost) — its C API is the actual native implementation, not a Python binding | 28.5k★, pushed the day of this check |
| `IsolationForest` | [isotree](https://github.com/david-cortes/isotree) — C++ core with C/R/Python bindings, benchmarked "at least 1 order of magnitude faster" than alternatives | 229★, pushed 3 weeks before this check, actively maintained |

**Revised decision: `ml_screen_discovery.py` and `ml_viability.py`'s training moves to native C++**, using the libraries above. This is an intentional **algorithm swap, not a byte-identical port** — XGBoost's tree-growing/regularization differs from sklearn's GradientBoostingClassifier, and isotree's isolation forest, while the same underlying algorithm family, won't reproduce sklearn's exact random splits. Validation for this piece is therefore **outcome-level, not float-diff**: compare discovered-screen quality and the walk-forward viability metrics (`ml_viability_5y.xlsx`'s hit-rate/edge columns) between the sklearn and C++ pipelines on the same historical data, not per-value numeric equality. The custom cross-entropy-method (CEM) policy-search loop in `ml_screen_discovery.py` (`np.random.normal` sampling + elite selection) was already simple, portable custom code, not a library dependency — it ports directly via `<random>`.

Data-acquisition (yfinance/nsepython-live/Trendlyne) remains the only genuine, permanent Python boundary — see the table above confirming no comparable native C++ replacement exists there, unlike the ML case.

## Explicitly Not Ported (with justification)

| Dropped | Why |
|---|---|
| Flink (`flink_cdc_consumer.py`, `flink_screens.py`) | Repo's own `kafka_cdc_consumer.py` docstring + INFRA.md document that PyFlink was fragile (Homebrew formula has no `brew services` entry, was killed by SIGTERM within an hour) and that the pure-Kafka consumer is the preferred/mandatory path. No C++ target exists anyway (JVM-only). Carry the repo's own decision forward. |
| Debezium / `cdc/` (Kafka Connect) | JVM-only; `market_store.py` already publishes CDC events directly and synchronously — Debezium would be a redundant second mechanism. |
| Playwright (the automation engine itself) | No C++ equivalent at Playwright's DOM/JS-rendering fidelity; `trendlyne_session.py` needs it specifically because Trendlyne is a JS-rendered SPA behind a reCAPTCHA-v3 login it deliberately avoids automating. |
| nsepython/NSEpy/bseindia/kabupy/pykrx *live* calls | Unofficial, session/anti-bot-protected scraping libraries with zero C++ equivalents; these are I/O-bound, not compute-bound, so there's no performance case for reimplementing them, only breakage risk. (NSE bhavcopy bulk EOD downloads are the exception — see the Python Boundary section — those are plain HTTP and port to libcurl like any other Phase-2/8 client.) |
| yfinance | Same reasoning — undocumented API surface, I/O-bound; verified via GitHub search that only thin/stale C++ wrappers exist (foxadb/yahoo-finance, 67★; puddup/yfinance-cpp, stale since 2023), not a real replacement — see "ML Training: Revised Decision" for how this same verification process changed the ML call. |

## Tooling & Library Choices

| Concern | Choice |
|---|---|
| Build / packages | CMake ≥3.28 + vcpkg (manifest mode) |
| Parquet/Arrow | Apache Arrow C++ (same format pyarrow already writes — zero translation) |
| Linear algebra | Eigen 3 (OLS via QR/SVD, pinv via `CompleteOrthogonalDecomposition`, ridge via `LDLT`) |
| Warehouse SQL | DuckDB's native C++ API (in-process, same engine `warehouse.py` calls) |
| Cassandra | DataStax `cassandra-cpp-driver` |
| Kafka | librdkafka (the same C library `confluent-kafka` wraps — closest possible parity) |
| SQLite | SQLite3 C API / SQLiteCpp |
| HTTP client | libcurl via `cpr` |
| HTTP server | Drogon (routing/JSON/async, substitutes FastAPI) |
| JSON | nlohmann/json |
| Date/calendar | HowardHinnant/date + a custom `TradingCalendar` (no drop-in for pandas `DatetimeIndex`/`Period`/ffill-snapping) |
| Testing | Catch2 v3 + CTest; Google Benchmark for perf claims |
| Gradient boosting | XGBoost (native C API) — replaces sklearn's `GradientBoostingClassifier` in `ml_screen_discovery.py` |
| Clustering / kernel PCA | mlpack (header-only) — replaces sklearn's `KMeans`; `PCA` itself needs no library, just Phase 1's Eigen-based `libs/linalg` |
| Isolation forest | isotree (C++ core) — replaces sklearn's `IsolationForest` |

## Repo Layout (new repo, `BazaarTalks-cpp`)

```
libs/calendar/        Phase 1 — trading calendar, date-bucketing, ffill-snap util
libs/stats/           Phase 1 — zscore, IC, monotonicity, rolling-window kernels (RSI/MACD/ADX/OBV/CMF)
libs/linalg/          Phase 1 — Eigen wrappers: OLS, ridge, pinv-based optimizers
libs/storage/         Phase 2 — duckdb_client, cassandra_client, sqlite_client, parquet_io
libs/streaming/       Phase 2 — kafka_client
libs/http/            Phase 2/8 — client (cpr), server (Drogon)
libs/quant_core/      Phase 3-6 — risk, cost_models, universe, factor, dvm, backtest
services/stream_consumers/   Phase 7
services/serve_api/          Phase 8
services/pipeline_cli/       Phase 9
tests/golden/ tests/unit/ tests/parity/
python-sidecar/       trimmed fork of current repo — scanners, scraping, ML training
```

## Validation Strategy (applies every phase)

Generate golden fixtures **from the current Python code, unmodified**: fixed input snapshots (parquet slice, SQLite snapshot) → run existing Python module → commit output as the golden fixture. C++ parity tests (Catch2) diff against these goldens:
- Pure arithmetic (risk/cost modules): `1e-9` relative tolerance.
- SVD/pinv-based optimizers, regressions: `1e-6` (documented per-module, not silently loosened on failure).
- Boolean/categorical inclusion logic (PIT filed-date filtering): **exact match**, not tolerance — a date being in/out of a point-in-time universe isn't a "close enough" property.

## Phased Plan

1. **Phase 0 — Scaffolding.** CMake/vcpkg/Catch2/CI skeleton, empty and green. *(done)*
2. **Phase 1 — Shared utility libs** (no external deps, unblocks everything): port `market_holidays.py` → `TradingCalendar`; build the ffill-date-snap utility replicating pandas' `get_indexer(method="ffill")` tie-breaking (this single semantic is reused by every PIT/backtest module in Phase 7 — get it right here with a dedicated golden test against an irregular trading-day index); port `marketdata.py`'s zscore/IC/monotonicity/trend_corr, the RSI/MACD/ADX/DMI/OBV/CMF rolling kernels; Eigen wrappers for OLS/pinv/ridge (match numpy's `rcond` tolerance convention explicitly). *(done — 24/24 tests passing)*
3. **Phase 2 — Storage/streaming clients**: DuckDB (port `warehouse.py`'s view-attachment SQL verbatim), Cassandra (port `market_store.py`'s mandatory-CDC-on-write semantics — test that a simulated broker-down condition throws, not degrades), Kafka (librdkafka producer/consumer, round-trip interop test against the existing Python consumers to prove wire-format compatibility before they're ever retired), SQLite, Parquet I/O (schema-equality check against pyarrow output — no accidental `float64`→`float32` or timestamp-tz drift). **Exit criterion: C++ can read every artifact Python currently writes and vice versa**, before any compute logic changes. *(in progress)*
4. **Phase 3 — Trivial pure-compute modules**: `risk.py`, `net_of_cost.py`/`apply_costs.py`, `survivorship.py`, `pit_panel.py` (filtering logic only), `meta_screen.py`, `fx.py`, `unlisted_valuation.py`, `crowding.py`, `hft_selection.py` (`np.polyfit`→ Phase 1's OLS/Vandermonde fit), `darvas_volume.py`. Golden-diff at `1e-9`. `pit_fundamentals.py`'s EDGAR HTTP/JSON fetch stays in the Python sidecar; only its as-of filtering logic ports. *(done — all 9 modules, 87/87 tests passing)*
5. **Phase 4 — OLS/portfolio-optimization/closed-form ML**: `factor_research.py`, `portfolio.py` (pinv optimizers + iterative cap/turnover-redistribution loops), `ml_signal_engine.py`'s Ridge path, `quality_factor.py` (careful manual reimplementation of `pd.get_dummies`' alphabetical reference-category convention for the fixed-effects regression — this is a named trap, not a routine port). `benchmark.py`: port only the regression math; leave the Ken-French CSV-in-zip fetch/parse in the sidecar, feeding parsed factor returns via a small parquet file. *(done — 119/119 tests passing; `quintile_table()`/`cross_section()` explicitly not ported, see factor_research's header)*
6. **Phase 4b — ML training stack (revised scope)**: `ml_screen_discovery.py`'s full pipeline (supervised GradientBoosting via XGBoost's C API, PCA via Eigen SVD, KMeans via mlpack, IsolationForest via isotree, the custom CEM policy-search loop via `<random>`) and `ml_viability.py`'s walk-forward retraining loop. Sequenced after Phase 4 since it depends on the same feature-matrix/z-score conventions. Validation is **outcome-level, not float-diff** (see "ML Training: Revised Decision") — compare discovered-screen quality and walk-forward hit-rate/edge against the sklearn pipeline's historical output, not per-value numeric equality. This phase adds three new dependencies (XGBoost, mlpack, isotree) — pull them in only when this phase actually starts, not earlier, to keep Phase 0-4's build lean.
7. **Phase 5 — Vectorized DVM/technical modules**: `dvm_global.py`, `dvm_engine.py`, `dvm_composite.py`. Preserve — do not silently "fix" — the documented snapshot-not-PIT limitation of these modules' fundamentals inputs. *(done — all 3 modules ported: 58 assertions/4 cases in `bt_dvm_global_tests`, 32/7 in `bt_dvm_engine_tests`, 28/5 in `bt_dvm_composite_tests`, plus a corrected `mfi()` golden test and a new `rolling_min()` in `bt_stats_tests`. 146/146 tests passing overall. See "Phase 5 notes" below for the divergences deliberately preserved between the three modules' near-but-not-quite-identical formulas.)*

   **Phase 5 notes:** all three `dvm_*.py` modules compute superficially similar Momentum/Durability metrics with genuinely different formulas, thresholds, and NaN-handling conventions — none of this port consolidates them into one shared implementation, since the Python originals themselves don't.

   `dvm_global.py` → `libs/quant_core/dvm_global`: two implementations of the same metrics exist — a per-ticker loop (`_tech()`) and the vectorised `process_market()` actually invoked by `main()`'s `ProcessPoolExecutor` fan-out. This port replicates `process_market()`'s exact behavior where the two diverge from each other or from the module's own docstring:
   - `golden_cross` is a single bar-back crossover check (`dma50.shift(1) <= dma200.shift(1)`), not the docstring's/`_tech()`'s "crossed in the last 5 sessions".
   - The `dist_52w` momentum sub-score (`100 + ((px/hi52-1)*100)*3`) is a different formula from `dvm_engine.py`'s `momentum_score()` (`100-(hi52/px-1)*300`) — not algebraically equivalent.
   - The vectorised beta is an internally inconsistent normalisation (population covariance ÷ sample (ddof=1) variance) — replicated verbatim.
   - `mfi()` (new in `libs/stats`) uses pandas' `.where(cond, 0.0)` semantics, where a NaN condition (the first bar) falls through to the 0.0 substitute in *both* the positive and negative flow sums — this was initially miscoded as a boolean-mask-assignment quirk (which would keep the raw value instead) based on a wrong assumption about the source; caught by the golden test itself failing (`48.417...` expected vs `48.585...` actual) and fixed by re-deriving from the real `dvm_global.py` source line, not from memory.
   - The 6 `SCREENS` predicates' `(r["field"] or 0)` idiom is NOT a NaN-to-zero substitution (Python's `or` only substitutes for a falsy left operand, and `bool(nan)` is `True`) — it's `std::isfinite(x) && x >= threshold`, which happens to produce the same outcome only because every current threshold is positive.

   `dvm_engine.py` → `libs/quant_core/dvm_engine`: `momentum_score()` (220-bar minimum, distinct from `dvm_global.py`'s 200) computes its 6 sub-scores as plain Python SCALARS via `min(100, max(0, x))` — Python's *builtin* min()/max(), which keep their first argument unless a later one compares strictly greater. Since NaN comparisons are always False, `max(0, nan)` returns 0 (not NaN), and the following `min(100, 0)` returns 0 — a NaN sub-score input silently collapses to 0. This is the opposite convention from `dvm_global.py`'s `process_market()`, which uses numpy's element-wise, properly NaN-propagating `np.minimum`/`np.maximum` on columnar arrays. Caught the same way as the `mfi()` bug above: an initial (wrong) implementation reused `dvm_global.py`'s NaN-propagating clip helper here too, and a golden test against the real Python output failed (`nan` vs `83.333...` expected) until re-derived from the actual scalar-clipping semantics. `durability_score()`'s point-in-time EDGAR fetch stays in the Python sidecar (Phase 6 territory); only its closed-form scoring formula (roe/de/fcf/rev-growth/Piotroski-F → one number) is ported, taking those five already-computed values as `std::optional` scalars.

   `dvm_composite.py` → `libs/quant_core/dvm_composite`: a *third*, independent `momentum()`/`durability()` pair. `momentum(c)` takes Close prices only (no ADX/volume sub-scores), uses a 200-bar minimum, `dvm_global.py`'s `dist_52w` formula and `rolling(252, min_periods=150)` window, and the same scalar Python-builtin-min/max NaN→0 convention as `dvm_engine.py`. `durability(r)` has its own 5 threshold tiers (no Piotroski term at all) and, unlike `dvm_engine.py`'s always-5-subs version, only averages whichever sub-scores have a *present* input — the mean is over 0–5 terms depending on data availability, and an all-absent row is NaN rather than a substituted-defaults score. `build_dvm_composite()` also ports `main()`'s cross-sectional earnings-yield/inverse-P/B valuation rank (computed once across every surviving stock from every market combined, not per-market) and preserves the ordering quirk that M/D/V are rounded to 1 decimal *before* the G/B classification and the composite average — a value that rounds across the 50 boundary changes classification versus using the unrounded figure.
8. **Phase 6 — PIT/backtest modules (correctness-critical gate)**: `pit_backtest.py`, `pit_global.py`, `screen_viability.py`. These are the platform's only genuinely lookahead-free layer — a subtle bug here (off-by-one in ffill-snap, or `filed <= date` vs `filed < date`) produces a backtest that still runs and looks plausible while being silently wrong. Validation must include, beyond golden-diff: (a) exact-match test on the set of tickers/rows included at each rebalance date, and (b) a **lookahead-injection test** — inject one fundamentals row dated one day after a rebalance date, assert output is identical to the fixture without that row. Gate: do not proceed to cutover until both pass against the multi-year fixture referenced in `PIT_BACKTEST_RESULTS.md`.
9. **Phase 7 — Streaming consumers**: `kafka_cdc_consumer.py`/`kafka_signal_consumer.py` (near-1:1 librdkafka port), `stream_pipeline.py`. Validate by running the C++ consumer against the same Kafka topic the existing Python producer publishes to.
10. **Phase 8 — HTTP serving layer**: port `serve.py`'s routes to Drogon (health/markets/screen/filter/ticker/chart/watchlists CRUD), `charts.py` (trivial SVG string builder), `dashboard.py`/`ticker_view.py`. **Security gate**: `validate_predicate`'s SQL-injection allow-list guard must not be transliterated casually — build a dedicated adversarial test suite (stacked queries, comment sequences, UNION/boolean-blind patterns) run against both the Python and C++ validators, asserting identical accept/reject decisions, since Python `re` and `std::regex`'s default grammars differ subtly enough to open a bypass that didn't exist in the original.
11. **Phase 9 — Orchestration and cutover**: `pipeline.py` as a C++ CLI orchestrator (last, depends on every prior phase). Run C++ and Python services side by side against the same storage substrate for at least one full multi-market scan cycle, comparing outputs continuously, before decommissioning the Python compute/serving paths. The Python sidecar itself is never decommissioned.

## New Strategies (research-derived, not ports)

Beyond the systematic Python→C++ port above, two modules were added directly in C++ based on a literature review of 5 trading-strategy papers (arXiv:2502.12537, 2412.15448, 2511.12120, 2411.07585, 2412.14361), requested separately from the port itself. 4 of the 5 papers propose deep-RL trading agents (PPO/A2C/DDPG/DQN with CNN feature extractors); their own reported results are weak-to-fragile (one RF-based paper fails to beat buy-and-hold; one DQN paper's "best" result is a single trade with 100% win rate; the papers themselves state "the literature lacks substantial evidence supporting the practical efficacy of RL agents"), and building any of them properly would require pulling a full neural-net training framework (e.g. libtorch) into this repo — a large, separate undertaking not justified by the evidence. Only the deterministic, closed-form pieces were built:

- **`libs/quant_core/momentum_breakout`** — the Keltner-Donchian channel-breakout momentum strategy from arXiv:2412.14361v1 (industry trend-following: combined-channel breakout entry, inverse-volatility position sizing, ratcheting trailing stop). Fully specified in the paper with no ML training required.
- **`libs/quant_core/turbulence_index`** — the Mahalanobis-distance market-turbulence/crash-detection overlay used by one of the ensemble-RL papers as a risk trigger; the index itself is pure linear algebra (reuses `bt_linalg::pinv`), extractable without any of that paper's RL machinery. Pairs with the existing `risk.hpp`'s `regime_flag()` as a cross-sectional (multi-asset) regime signal.

**Validation note:** unlike every ported module, these have no existing Python original to generate golden fixtures from. Tests cross-check the formulas against an independent numpy/pandas implementation of the same published equations, documented per-test as such — not "the actual Python output." The source papers do not state baseline numeric values for the strategy parameters (Keltner window/multiplier, target volatility) either; every parameter is a caller-supplied input in the C++ API, not a hardcoded "recommended" constant.

## Non-Goals

- ONNX-exported inference: moot now that Phase 4b moves training itself to native C++ (XGBoost/mlpack/isotree) — there's no Python-trained model left needing an inference bridge.
- Reimplementing Flink, Debezium, or Playwright natively in C++ — no viable C++ target exists for any of them (see table above; verified via GitHub search — no lightweight native-C++ Chrome DevTools Protocol client exists either, only JS/Rust/Go/Nim clients, so a from-scratch CDP client remains real, uncredited engineering effort, not a "just link a library" option).
- Reimplementing NSE's *live* session-scraped endpoints (VIX/FII-DII/bulk-deals) in C++ — confirmed via GitHub search that zero C++ libraries exist for this (only Python/Go/JS bhavcopy tools, all for the separate bulk-EOD-file case which already ports fine). Reverse-engineering the live, anti-bot-protected endpoints directly would be fragile with no offsetting benefit versus keeping `nse_data_fetcher.py`'s live-call path in the sidecar.

## Verification

- Phase 2 exit gate: round-trip read/write of every artifact type (parquet, SQLite tables, Cassandra rows, Kafka messages) between the Python and C++ sides.
- Phase 6 exit gate (hardest): exact-match inclusion-set tests + lookahead-injection tests pass on the real multi-year PIT backtest fixture, not just a synthetic case — treat as a required, blocking CI check on any PR touching the backtest library.
- Phase 8 exit gate: adversarial SQL-injection test suite passes identically on old (Python) and new (C++) `validate_predicate`.
- Overall cutover gate: C++ and Python stacks run in parallel for ≥1 full scan cycle with continuously diffed outputs before any Python compute/serving code is removed.
- CI mirrors the existing repo's governance philosophy: build+test, a phase-boundary/layering check (quant_core/backtest never links against services/*, no C++ target references python-sidecar/), and the existing SHA-256 checksum-manifest integrity check, plus a required "parity-gate" job specifically for Phase 6 changes.

## Critical Files (Python originals to port from)

- `market_store.py`, `warehouse.py` — storage contracts (Phase 2)
- `pit_backtest.py`, `pit_fundamentals.py`, `pit_panel.py`, `pit_global.py` — correctness-critical PIT logic (Phase 6)
- `serve.py` (esp. `validate_predicate`) — serving + security gate (Phase 8)
- `dvm_global.py` — template for vectorized-module porting (Phase 5)
- `kafka_cdc_consumer.py` — streaming consumer pattern + repo's own Flink-vs-Kafka decision record (Phase 7 / Non-Goals)
- `requirements.txt`, `.github/workflows/ci.yml` — confirms mandatory vs optional infra deps and the existing governance/CI pattern to mirror
