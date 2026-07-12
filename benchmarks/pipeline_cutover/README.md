# Pipeline cutover benchmark

Timing/accuracy comparison between `pipeline.py` (Python original) and
`bt_pipeline_cli` (C++ port) against real production data, across every
market with real DVM-scored data. See `docs/MIGRATION_PLAN.md`'s "Cutover
run #4" section for the full write-up and findings.

## Usage

```bash
python3 bench_python.py   # writes analyze/process timing+output to stdout as JSON
python3 bench_cpp.py       # same, for bt_pipeline_cli
```

Both default to this machine's real deployment layout (`~/BazaarTalks`,
`~/market-data-artifacts/seed_ohlc`) and a scratch workdir under the system
temp dir. Override via the `BAZAARTALKS_REPO`, `BAZAARTALKS_OHLC_SEED`,
`BAZAARTALKS_CPP_BIN` env vars (see each script's docstring) or by passing a
workdir path as the first argument, to run this against a different
checkout/data location.

`report_2026-07-12.html` is the rendered comparison from the run documented
in the migration plan -- open it directly in a browser.
