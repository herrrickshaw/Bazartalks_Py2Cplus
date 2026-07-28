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

**For anything timing-sensitive (benchmarks, production deployment), use
`--preset release` instead of `debug`.** The `debug` preset (`-O0`, full
debug symbols) produces a large unstripped binary whose fixed per-process
startup cost alone was measured at ~0.28s — enough to make a properly-ported
C++ CLI look slower than the Python original purely from binary size/link
overhead, not engine speed. `release` (`CMAKE_BUILD_TYPE=Release`) drops that
to ~0.01s; see `docs/MIGRATION_PLAN.md`'s Cutover run #4 correction for the
full before/after benchmark. `ci-build` (`RelWithDebInfo`) is an equally fast
alternative when debug symbols are still wanted.

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
benchmarks/        cutover benchmark drivers + rendered reports (see docs/MIGRATION_PLAN.md)
ops/               burn-in log rotation/backup scripts (launchd-driven, see plists at repo root)
```

Every C++ module here is validated against golden fixtures generated from
the *existing, unmodified* Python originals (see each test file's header
comment for the exact `python3 -c "..."` command used to produce the
reference values it checks against).

<!-- 
DATA LIBRARY LINK - Add this section to every repo README.md
This snippet provides discovery and documentation links.
-->

## 📊 Data Discovery

This repository is part of the **Global Data Library** — a unified catalog of 10,528 datasets across 40+ repositories.

### Quick Links

- **[Global Data Library README](.ruflo/DATA_LIBRARY_README.md)** — Full catalog, search API, and usage examples
- **[Data Library Python Interface](.ruflo/data-library/data_library.py)** — Query datasets programmatically
- **[Repository Scanner](.ruflo/data-library/repo_scanner.py)** — Reindex all repos to update the catalog

### Datasets in This Repository

The data catalog automatically inventories all datasets in this repo. To find your data:

```python
from data_library import DataLibrary

lib = DataLibrary()

# Search this repo's datasets
results = lib.search("", source="<repo-name>")

# Get dataset details
dataset = lib.get("<dataset_id>")
print(f"Rows: {dataset['row_count']}")
print(f"Freshness: {dataset['freshness_hours']} hours old")
print(f"Storage: {dataset['storage_tier']}")
```

### Browse the Full Catalog

**Market Coverage** (5 markets, 21,279 symbols):
- India (NSE/BSE): 2,364 instruments
- US (NASDAQ/NYSE): 7,442 instruments
- Europe (17 exchanges): 1,214 instruments
- Japan (TSE): 3,709 instruments
- Korea (KRX): 2,768 instruments

**Government Sources** (30+ ministries):
- MOSPI: 25 datasets (GDP, CPI, trade, agri, power)
- SEBI: 151,928 XBRL results + IPO pipeline
- PIB: 25+ ministry announcements
- DGFT: India trade data (monthly)
- Agmarknet: 300+ mandi prices (daily)
- NSE/MCX: Real-time derivatives chains

See [Global Data Library README](.ruflo/DATA_LIBRARY_README.md) for complete documentation.

### Finding Data Across All Repos

```python
# Find India OHLCV data (might be in multiple repos)
lib.search("india ohlcv", market="india")

# Get the fastest/freshest version
optimal = lib.get_optimal("india ohlcv", latency="<100ms", freshness="<1day")
# Returns: {"storage_tier": "cassandra", "path": "..."}

# Check data gaps
gaps = lib.gaps("india", date_from="2026-01-01")

# See which collectors are stale
status = lib.collectors_status()
```

---
