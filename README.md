# BazaarTalks-cpp

C++ reimplementation of [BazaarTalks](https://github.com/herrrickshaw/BazaarTalks)'s storage,
streaming, factor/quant compute, backtest, and HTTP serving layers. See
`docs/MIGRATION_PLAN.md` for the full phased plan, the Python boundary
decision, and the validation strategy this repo follows.

Scope note: this is a **hybrid** rewrite, not a uniform one. Everything with
a mature native C++ path (DuckDB, Cassandra, Kafka, SQLite, linear algebra,
factor math, backtests, HTTP serving) lives here. Everything with no C++
equivalent (yfinance, nsepython/bseindia/kabupy/pykrx scraping,
Playwright-driven Trendlyne access, sklearn's GradientBoosting/PCA/KMeans/
IsolationForest training) stays in `python-sidecar/` permanently by design —
see the migration plan's "Explicitly Not Ported" section for the reasoning.

## Build

```sh
git clone --depth 1 https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Layout

```
libs/calendar/     Phase 1 -- trading calendar, ffill date-snap utility
libs/stats/        Phase 1 -- cross-sectional stats (zscore, IC, monotonicity, trend_corr)
libs/linalg/       Phase 1 -- Eigen-based OLS/pinv/ridge wrappers
libs/storage/      Phase 2 -- DuckDB/Cassandra/SQLite/Parquet clients
libs/streaming/    Phase 2/7 -- Kafka (librdkafka) client
libs/http/         Phase 2/8 -- HTTP client (cpr) + server (Drogon)
libs/quant_core/   Phase 3-6 -- risk/cost/universe/factor/dvm/backtest modules
services/          Phase 7-9 -- stream consumers, serve_api, pipeline_cli
tests/             golden/unit/parity test fixtures and adversarial security suites
python-sidecar/    permanent Python acquisition + ML-training service (never ported)
```

Every C++ module here is validated against golden fixtures generated from
the *existing, unmodified* Python originals (see each test file's header
comment for the exact `python3 -c "..."` command used to produce the
reference values it checks against).
