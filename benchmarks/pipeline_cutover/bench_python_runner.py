#!/usr/bin/env python3
"""
Single-invocation runner: applies the same warehouse.SEED/DB monkeypatch as
bench_python.py, then calls the REAL pipeline.stage_process()/stage_analyze()
production functions exactly once, for exactly one stage -- launched fresh
by bench_python.py (subprocess per call) so process-startup overhead is
counted symmetrically with bench_cpp.py's fresh-process-per-call pattern,
instead of comparing a warm long-lived Python process against a cold C++
process per market.

Usage: bench_python_runner.py --process
       bench_python_runner.py --analyze MARKET

Env vars (all optional, default to this machine's real deployment layout):
  BAZAARTALKS_REPO         path to the BazaarTalks checkout (has pipeline.py/warehouse.py)
  BAZAARTALKS_OHLC_SEED    real OHLC parquet dir (warehouse.py's SEED, repointed here
                           since the production constant's own path can go stale --
                           see docs/MIGRATION_PLAN.md's Cutover run #4 notes)
  BAZAARTALKS_BENCH_DUCKDB scratch DuckDB file path (set by bench_python.py)
"""
import json
import os
import sys

REPO = os.environ.get("BAZAARTALKS_REPO", os.path.expanduser("~/BazaarTalks"))
sys.path.insert(0, REPO)

import duckdb
import warehouse
import pipeline

warehouse.SEED = os.environ.get("BAZAARTALKS_OHLC_SEED", os.path.expanduser("~/market-data-artifacts/seed_ohlc"))
warehouse.DB = os.environ.get("BAZAARTALKS_BENCH_DUCKDB", "/tmp/bazaartalks_bench/py_bench.duckdb")

mode = sys.argv[1]

if mode == "--process":
    ok = pipeline.stage_process()
    print(json.dumps({"ok": ok}))
elif mode == "--analyze":
    market = sys.argv[2]
    # Mirrors stage_analyze()'s DVM-only SQL exactly, skipping its trailing
    # accumulation_screener.py call (live yfinance scan, no C++ port -- see
    # bench_python.py's docstring for why that's excluded from a native
    # engine-speed comparison).
    con = duckdb.connect(warehouse.DB)
    warehouse.build(con)
    momentum = con.execute(
        "SELECT count(*) scored, round(avg(M),1) avg_M FROM dvm_global WHERE market = ?",
        [market],
    ).fetchone()
    classification = con.execute(
        "SELECT code, label, count(*) n FROM dvm_composite WHERE market = ? "
        "GROUP BY 1,2 ORDER BY n DESC",
        [market],
    ).fetchall()
    con.close()
    print(json.dumps({
        "scored": momentum[0],
        "avg_M": momentum[1],
        "classification": [{"code": c, "label": l, "n": n} for c, l, n in classification],
    }))
else:
    sys.exit(f"unknown mode {mode}")
