# python-sidecar

Permanent Python service — never ported to C++. See
`../docs/MIGRATION_PLAN.md` ("Decision: The Python Boundary" and "Explicitly
Not Ported") for the reasoning.

Owns everything with no C++ equivalent:

- Market-data acquisition: yfinance, `nsepython`/`NSEpy`/`nse`-bhavcopy,
  `bseindia`, `kabupy`, `pykrx`.
- Session-based scraping: `trendlyne_session.py` (Playwright — Trendlyne is
  a JS-rendered SPA behind a reCAPTCHA-v3 login), `screener_session.py`.
- `nse_data_fetcher.py`'s India live-data facade.
- ML training: `ml_screen_discovery.py` (GradientBoosting/PCA/KMeans/
  IsolationForest/CEM policy search), `ml_viability.py`'s sklearn training
  path.
- Cheap-to-keep scraping glue: EDGAR/CVM/GLEIF-type fetchers, the Ken French
  factor-library CSV-in-zip parser used by `benchmark.py`.

Its only contract with the C++ core is **file-based**: it writes to the
same parquet layout, `fundamentals_cache.db`/`edgar_facts.db` SQLite
schemas, and Cassandra `market.ohlc_bars` table the C++ storage clients in
`libs/storage/` read from. No new IPC protocol, no shared build, no
`#include` of anything under this directory from C++ code — enforced by
`scripts/check_layering.py` in CI.

This directory will be populated with a trimmed fork of the modules listed
above from the original `herrrickshaw/BazaarTalks` repo (dropping every
module that Phase 3+ ports natively to C++).
