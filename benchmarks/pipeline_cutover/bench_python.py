#!/usr/bin/env python3
"""
Timing/accuracy benchmark driver -- subprocess-invokes bench_python_runner.py
fresh per call (--process once, --analyze once per market), matching
bench_cpp.py's fresh-process-per-call pattern exactly so process-startup
overhead is counted symmetrically on both sides. See bench_python_runner.py
for what production code actually runs inside each call.

Usage: python3 bench_python.py [workdir]
  workdir: scratch directory for the runner's DuckDB file
           (default: a bazaartalks_bench/ dir under the system temp dir)
"""
import json
import os
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
WORKDIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(tempfile.gettempdir(), "bazaartalks_bench")
os.makedirs(WORKDIR, exist_ok=True)

RUNNER = os.path.join(HERE, "bench_python_runner.py")
DUCKDB = os.path.join(WORKDIR, "py_bench.duckdb")
if os.path.exists(DUCKDB):
    os.remove(DUCKDB)

env = {**os.environ, "BAZAARTALKS_BENCH_DUCKDB": DUCKDB}

MARKETS = ["US", "CN", "JP", "KR", "TW", "CA", "AU", "HK", "UK", "EU",
           "SG", "SE", "DE", "SA", "BR", "CH", "ZA", "FI", "DK"]

results = {"process_time_s": None, "analyze": {}}

t0 = time.time()
p = subprocess.run(["python3", RUNNER, "--process"], capture_output=True, text=True, env=env)
results["process_time_s"] = time.time() - t0
assert p.returncode == 0, p.stderr

for market in MARKETS:
    t0 = time.time()
    p = subprocess.run(["python3", RUNNER, "--analyze", market], capture_output=True, text=True, env=env)
    elapsed = time.time() - t0
    assert p.returncode == 0, p.stderr
    payload = json.loads(p.stdout)
    results["analyze"][market] = {"time_s": elapsed, **payload}

print(json.dumps(results, indent=2))
