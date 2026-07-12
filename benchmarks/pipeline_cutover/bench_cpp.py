#!/usr/bin/env python3
"""
Timing/accuracy benchmark driver for bt_pipeline_cli, mirroring
bench_python.py's methodology exactly (same real data, same per-market
warehouse rebuild-per-analyze-call behavior, same time.time() wall-clock
measurement) so the two numbers are directly comparable.

Usage: python3 bench_cpp.py [workdir]

Env vars (all optional, default to this machine's real deployment layout):
  BAZAARTALKS_CPP_BIN    path to the built bt_pipeline_cli binary
  BAZAARTALKS_REPO       path to the BazaarTalks checkout (has the *.db files
                         and cache_seed_local/)
  BAZAARTALKS_OHLC_SEED  real OHLC parquet dir (see bench_python_runner.py's
                         docstring for why the production default can go stale)
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
WORKDIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(tempfile.gettempdir(), "bazaartalks_bench")
os.makedirs(WORKDIR, exist_ok=True)

REPO = os.environ.get("BAZAARTALKS_REPO", os.path.expanduser("~/BazaarTalks"))
BIN = os.environ.get(
    "BAZAARTALKS_CPP_BIN",
    os.path.join(HERE, "..", "..", "build", "debug", "services", "pipeline_cli", "bt_pipeline_cli"),
)
OHLC_SEED = os.environ.get("BAZAARTALKS_OHLC_SEED", os.path.expanduser("~/market-data-artifacts/seed_ohlc"))
DUCKDB = os.path.join(WORKDIR, "cpp_bench.duckdb")
if os.path.exists(DUCKDB):
    os.remove(DUCKDB)

COMMON_ARGS = [
    "--duckdb", DUCKDB,
    "--ohlc-dir", OHLC_SEED,
    "--ohlc-dir", os.path.join(REPO, "cache_seed_local"),
    "--fundamentals-db", os.path.join(REPO, "fundamentals_cache.db"),
    "--dvm-global-db", os.path.join(REPO, "dvm_global.db"),
    "--dvm-composite-db", os.path.join(REPO, "dvm_composite.db"),
    "--viability-db", os.path.join(REPO, "viability_summary.db"),
]

MARKETS = ["US", "CN", "JP", "KR", "TW", "CA", "AU", "HK", "UK", "EU",
           "SG", "SE", "DE", "SA", "BR", "CH", "ZA", "FI", "DK"]

results = {"process_time_s": None, "analyze": {}}

t0 = time.time()
p = subprocess.run([BIN, "--process"] + COMMON_ARGS, capture_output=True, text=True)
results["process_time_s"] = time.time() - t0
assert p.returncode == 0, p.stderr

momentum_re = re.compile(r"scored=(\d+) avg_M=([\d.]+|NULL)")
class_re = re.compile(r"code=(\S+) label=(.+?) n=(\d+)")

for market in MARKETS:
    t0 = time.time()
    p = subprocess.run([BIN, "--analyze", market] + COMMON_ARGS, capture_output=True, text=True)
    elapsed = time.time() - t0
    assert p.returncode == 0, p.stderr

    m = momentum_re.search(p.stdout)
    scored = int(m.group(1)) if m else None
    avg_M = None if (m is None or m.group(2) == "NULL") else round(float(m.group(2)), 1)

    classification = []
    for cm in class_re.finditer(p.stdout):
        classification.append({"code": cm.group(1), "label": cm.group(2), "n": int(cm.group(3))})

    results["analyze"][market] = {
        "time_s": elapsed,
        "scored": scored,
        "avg_M": avg_M,
        "classification": classification,
    }

print(json.dumps(results, indent=2))
