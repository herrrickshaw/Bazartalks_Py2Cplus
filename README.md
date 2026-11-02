# BazaarTalks-cpp

C++20 reimplementation of the storage, streaming, factor/quant compute, backtest and HTTP-serving layers of
[BazaarTalks](https://github.com/herrrickshaw/BazaarTalks), a multi-market equity research platform. It builds
static libraries, stream-consumer services, a Drogon HTTP API and a pipeline CLI. Parts with no native C++
equivalent (scraping, sklearn-style ML training) stay in `python-sidecar/` by design.

> Status: active. Research code; not investment advice. No licence file yet; all rights reserved.

<!-- how-to-use-with-ai -->
New here? See [How to use this repo with AI](HOW_TO_USE_WITH_AI.md).

## What it does
- Ports pure-compute quant modules (DVM scoring, quality factor, Ridge signal engine, PIT backtest, portfolio caps,
  HFT selection, screens) to C++, each validated against golden values from the unmodified Python originals.
- Provides storage clients (DuckDB, Cassandra, SQLite, Parquet, Databento) and a Kafka client.
- Runs Kafka consumers (CDC, signal, ticker-signal pipeline) and a Drogon HTTP API (`services/serve_api`).
- Provides `bt_pipeline_cli`: native `--process` / `--analyze`; `--source` / `--validate` / `--graphics` / `--live`
  shell out to the Python sidecar.

## Quick start
```bash
git clone https://github.com/herrrickshaw/Bazartalks_Py2Cplus.git
cd Bazartalks_Py2Cplus
git clone --depth 1 https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --preset release && cmake --build --preset release
```
Use `--preset release` (or `ci-build`) for anything timing-sensitive. The `debug` preset (`-O0`) adds roughly
0.28 s of fixed process-startup cost per CLI call and once made the C++ CLI look slower than Python.

## How it works
Phased port, lowest layers first (see `docs/MIGRATION_PLAN.md` for the plan, boundary decision and validation):

| Phase | Path | Output |
|---|---|---|
| 1 | `libs/calendar`, `libs/stats`, `libs/linalg` | calendar, cross-sectional stats, indicators, OLS/ridge (Eigen) |
| 2/7 | `libs/storage/*`, `libs/streaming/kafka_client` | DuckDB/Cassandra/SQLite/Parquet/Databento and Kafka clients |
| 3-6 | `libs/quant_core/*` | risk, cost, universe, factor, DVM, backtest, charts, ticker view |
| 7 | `services/stream_consumers` | CDC, signal and ticker-signal consumers |
| 8 | `services/serve_api` | Drogon HTTP server |
| 9 | `services/pipeline_cli` | `--source/--validate/--process/--analyze/--graphics` orchestrator |

The C++ core and the sidecar share only files (Parquet, SQLite, Cassandra tables), never headers or IPC;
`scripts/check_layering.py` enforces this in CI.

## Repository layout
| Path | Purpose |
|---|---|
| `libs/` | calendar, stats, linalg, quant_core, storage, streaming libraries |
| `services/` | stream consumers, serve_api, pipeline_cli, databento_ingest |
| `python-sidecar/` | permanent Python acquisition + ML-training service (see its README) |
| `benchmarks/pipeline_cutover/` | Python-vs-C++ benchmark drivers and rendered report |
| `ops/`, `*.plist` | burn-in Kafka log rotation/backup scripts and launchd units |
| `scripts/` | `check_layering.py`, `verify_integrity.sh` |
| `docs/MIGRATION_PLAN.md` | phased plan, Python boundary, cutover runs |

Tests live beside each library (`libs/*/tests`, `libs/quant_core/*/tests`).

## Configuration
CMake presets: `debug`, `ci-build` (RelWithDebInfo), `release`; the toolchain is `vcpkg/` in the repo root.
Dependencies are in `vcpkg.json`; `cassandra-cpp-driver` has no vcpkg port and comes from Homebrew.
`bt_pipeline_cli` takes `--ohlc-dir`, `--companies-parquet`, `--fundamentals-db`, `--dvm-global-db`,
`--dvm-composite-db`, `--viability-db`. Vendor API keys (e.g. Databento) are read at run time and never committed.

## Testing
```bash
ctest --preset debug          # full suite; Kafka/Cassandra-tagged tests need local brokers
python3 scripts/check_layering.py
```
Pure-compute libraries (calendar, stats, linalg, quant_core) need only Eigen, date, nlohmann-json and Catch2;
`libs/*` can be built that way without vcpkg's DuckDB/Arrow/Drogon (177 tests).

## Limitations and known issues
- Reported benchmark: 5.94x end-to-end speedup (Python 9.52 s vs C++ release 1.60 s, 19 markets, one process per
  call), with 19/19 exact output matches. Measured by the repo owner's `benchmarks/`; not re-measured here.
- `services/*/main.cpp` files are plumbing without unit tests by design.
- Integration tests need live Kafka and Cassandra.

## Related repositories
- [BazaarTalks](https://github.com/herrrickshaw/BazaarTalks): the Python original this ports.

## Licence
No licence file yet; all rights reserved.
