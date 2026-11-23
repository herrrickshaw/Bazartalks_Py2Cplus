"""The three benchmark drivers are scripts that run at import time. They are executed with the
real source (compiled under its real filename for coverage) in a sandbox: subprocess.run is faked,
workdirs live in tmp_path, nothing touches a real database, binary or checkout."""
import json
import os
import subprocess
import sys
import types

import pytest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BENCH = os.path.join(REPO, "benchmarks", "pipeline_cutover")
MARKETS = ["US", "CN", "JP", "KR", "TW", "CA", "AU", "HK", "UK", "EU", "SG", "SE", "DE", "SA", "BR", "CH", "ZA", "FI", "DK"]


def execute(name, monkeypatch, argv, env=None, run=None):
    for k, v in (env or {}).items():
        monkeypatch.setenv(k, v)
    monkeypatch.setattr(sys, "argv", [name, *argv])
    if run is not None:
        monkeypatch.setattr(subprocess, "run", run)
    path = os.path.join(BENCH, name)
    ns = {"__name__": f"bench_{name}", "__file__": path}
    exec(compile(open(path, encoding="utf-8").read(), path, "exec"), ns)
    return ns


def proc(stdout="", rc=0, stderr=""):
    return types.SimpleNamespace(returncode=rc, stdout=stdout, stderr=stderr)


# ---------------- bench_cpp.py ----------------
def cpp_run(calls, fail_on=None):
    def run(cmd, capture_output, text):
        calls.append(cmd)
        if fail_on and fail_on in cmd:
            return proc(rc=3, stderr="boom")
        if "--process" in cmd:
            return proc("processed\n")
        mk = cmd[cmd.index("--analyze") + 1]
        if mk == "US":
            return proc("momentum: scored=120 avg_M=55.26\nclass: code=A label=Strong buy n=7\n"
                        "class: code=B label=Hold n=3\n")
        if mk == "CN":
            return proc("scored=0 avg_M=NULL\n")
        return proc("no useful output\n")
    return run


def test_bench_cpp_json_and_command_lines(tmp_path, monkeypatch, capsys):
    calls = []
    work = tmp_path / "work"
    repo = tmp_path / "repo"
    ns = execute("bench_cpp.py", monkeypatch, [str(work)],
                 env={"BAZAARTALKS_REPO": str(repo), "BAZAARTALKS_CPP_BIN": "/bin/cli", "BAZAARTALKS_OHLC_SEED": "/seed"},
                 run=cpp_run(calls))
    out = json.loads(capsys.readouterr().out)
    assert out["process_time_s"] >= 0 and list(out["analyze"]) == MARKETS
    us, cn, jp = out["analyze"]["US"], out["analyze"]["CN"], out["analyze"]["JP"]
    assert us["scored"] == 120 and us["avg_M"] == 55.3 and us["classification"] == [
        {"code": "A", "label": "Strong buy", "n": 7}, {"code": "B", "label": "Hold", "n": 3}]
    assert cn["scored"] == 0 and cn["avg_M"] is None and cn["classification"] == []
    assert jp["scored"] is None and jp["avg_M"] is None
    assert calls[0][:2] == ["/bin/cli", "--process"] and calls[1][:3] == ["/bin/cli", "--analyze", "US"]
    assert len(calls) == 1 + len(MARKETS)
    common = calls[0][2:]
    assert all(c[3:] == common for c in calls[1:])                      # every analyze call repeats the same data arguments
    assert common == ["--duckdb", str(work / "cpp_bench.duckdb"), "--ohlc-dir", "/seed", "--ohlc-dir", str(repo / "cache_seed_local"),
                      "--fundamentals-db", str(repo / "fundamentals_cache.db"), "--dvm-global-db", str(repo / "dvm_global.db"),
                      "--dvm-composite-db", str(repo / "dvm_composite.db"), "--viability-db", str(repo / "viability_summary.db")]
    assert work.is_dir() and ns["MARKETS"] == MARKETS


def test_bench_cpp_removes_stale_duckdb(tmp_path, monkeypatch, capsys):
    work = tmp_path / "w"
    work.mkdir()
    (work / "cpp_bench.duckdb").write_text("stale")
    execute("bench_cpp.py", monkeypatch, [str(work)], env={"BAZAARTALKS_CPP_BIN": "/bin/cli"}, run=cpp_run([]))
    assert not (work / "cpp_bench.duckdb").exists()


def test_bench_cpp_defaults_when_no_env(tmp_path, monkeypatch, capsys):
    for k in ("BAZAARTALKS_REPO", "BAZAARTALKS_CPP_BIN", "BAZAARTALKS_OHLC_SEED"):
        monkeypatch.delenv(k, raising=False)
    monkeypatch.setenv("HOME", str(tmp_path / "home"))
    monkeypatch.setattr(sys, "argv", ["bench_cpp.py"])
    monkeypatch.setattr(subprocess, "run", cpp_run([]))
    import tempfile
    monkeypatch.setattr(tempfile, "gettempdir", lambda: str(tmp_path / "tmp"))
    path = os.path.join(BENCH, "bench_cpp.py")
    ns = {"__name__": "x", "__file__": path}
    exec(compile(open(path, encoding="utf-8").read(), path, "exec"), ns)
    assert ns["WORKDIR"] == str(tmp_path / "tmp" / "bazaartalks_bench")
    assert ns["REPO"] == str(tmp_path / "home" / "BazaarTalks")
    assert ns["BIN"].endswith(os.path.join("build", "release", "services", "pipeline_cli", "bt_pipeline_cli"))
    assert ns["OHLC_SEED"] == str(tmp_path / "home" / "market-data-artifacts" / "seed_ohlc")


@pytest.mark.parametrize("fail_on", ["--process", "US"])
def test_bench_cpp_failing_binary_aborts_with_stderr(tmp_path, monkeypatch, fail_on):
    with pytest.raises(AssertionError, match="boom"):
        execute("bench_cpp.py", monkeypatch, [str(tmp_path / "w")], env={"BAZAARTALKS_CPP_BIN": "/bin/cli"},
                run=cpp_run([], fail_on=fail_on))


# ---------------- bench_python.py ----------------
def test_bench_python_invokes_runner_per_stage(tmp_path, monkeypatch, capsys):
    calls = []

    def run(cmd, capture_output, text, env):
        calls.append((cmd, env))
        if "--process" in cmd:
            return proc('{"ok": true}')
        return proc(json.dumps({"scored": 5, "avg_M": 1.5, "classification": []}))

    work = tmp_path / "w"
    work.mkdir()
    (work / "py_bench.duckdb").write_text("stale")
    execute("bench_python.py", monkeypatch, [str(work)], run=run)
    out = json.loads(capsys.readouterr().out)
    assert list(out["analyze"]) == MARKETS and out["analyze"]["US"]["scored"] == 5 and out["analyze"]["US"]["time_s"] >= 0
    cmd0, env0 = calls[0]
    assert cmd0 == ["python3", os.path.join(BENCH, "bench_python_runner.py"), "--process"]
    assert env0["BAZAARTALKS_BENCH_DUCKDB"] == str(work / "py_bench.duckdb")
    assert calls[1][0][2:] == ["--analyze", "US"] and len(calls) == 1 + len(MARKETS)
    assert not (work / "py_bench.duckdb").exists()


def test_bench_python_process_failure_asserts_before_any_analyze(tmp_path, monkeypatch):
    calls = []

    def run(cmd, capture_output, text, env):
        calls.append(cmd)
        return proc(rc=4, stderr="process died") if "--process" in cmd else proc('{"scored": 1}')

    with pytest.raises(AssertionError, match="process died"):
        execute("bench_python.py", monkeypatch, [str(tmp_path)], run=run)
    assert len(calls) == 1


def test_bench_python_analyze_failure_asserts(tmp_path, monkeypatch):
    def run(cmd, capture_output, text, env):
        return proc('{"ok": 1}') if "--process" in cmd else proc(rc=2, stderr="analyze died")

    with pytest.raises(AssertionError, match="analyze died"):
        execute("bench_python.py", monkeypatch, [str(tmp_path)], run=run)


def test_bench_python_empty_runner_output_is_an_error(tmp_path, monkeypatch):
    def run(cmd, capture_output, text, env):
        return proc('{"ok": 1}') if "--process" in cmd else proc("")

    with pytest.raises(json.JSONDecodeError):
        execute("bench_python.py", monkeypatch, [str(tmp_path)], run=run)


def test_bench_python_failure_asserts(tmp_path, monkeypatch):
    with pytest.raises(AssertionError, match="runner died"):
        execute("bench_python.py", monkeypatch, [str(tmp_path)], run=lambda *a, **k: proc(rc=1, stderr="runner died"))


def test_bench_python_default_workdir(tmp_path, monkeypatch, capsys):
    import tempfile
    monkeypatch.setattr(tempfile, "gettempdir", lambda: str(tmp_path / "t"))
    ns = execute("bench_python.py", monkeypatch, [], run=lambda cmd, **k: proc('{"ok": 1}' if "--process" in cmd else '{"scored": 1}'))
    assert ns["WORKDIR"] == str(tmp_path / "t" / "bazaartalks_bench")


# ---------------- bench_python_runner.py ----------------
class FakeCon:
    def __init__(self, log):
        self.log = log

    def execute(self, sql, params):
        self.log.append((" ".join(sql.split()), params))
        return types.SimpleNamespace(fetchone=lambda: (42, 61.5), fetchall=lambda: [("A", "Strong", 9), ("B", "Weak", 2)])

    def close(self):
        self.log.append("closed")


@pytest.fixture
def fake_stack(monkeypatch):
    log = []
    duck = types.ModuleType("duckdb")
    duck.connect = lambda path: (log.append(("connect", path)), FakeCon(log))[1]
    wh = types.ModuleType("warehouse")
    wh.SEED, wh.DB = "orig-seed", "orig-db"
    wh.build = lambda con: log.append("build")
    pipe = types.ModuleType("pipeline")
    pipe.stage_process = lambda: (log.append("stage_process"), True)[1]
    for m in (duck, wh, pipe):
        monkeypatch.setitem(sys.modules, m.__name__, m)
    return log, wh


def test_runner_process_mode(tmp_path, monkeypatch, capsys, fake_stack):
    log, wh = fake_stack
    monkeypatch.delenv("BAZAARTALKS_REPO", raising=False)
    monkeypatch.setenv("BAZAARTALKS_OHLC_SEED", "/real/seed")
    monkeypatch.setenv("BAZAARTALKS_BENCH_DUCKDB", "/tmp/x.duckdb")
    saved = list(sys.path)
    execute("bench_python_runner.py", monkeypatch, ["--process"])
    assert json.loads(capsys.readouterr().out) == {"ok": True}
    assert (wh.SEED, wh.DB) == ("/real/seed", "/tmp/x.duckdb") and log == ["stage_process"]
    assert sys.path[0] == os.path.expanduser("~/BazaarTalks")
    sys.path[:] = saved


def test_runner_analyze_mode(tmp_path, monkeypatch, capsys, fake_stack):
    log, wh = fake_stack
    monkeypatch.setenv("BAZAARTALKS_REPO", "/repo")
    monkeypatch.setenv("BAZAARTALKS_BENCH_DUCKDB", "/tmp/y.duckdb")
    monkeypatch.delenv("BAZAARTALKS_OHLC_SEED", raising=False)
    saved = list(sys.path)
    execute("bench_python_runner.py", monkeypatch, ["--analyze", "JP"])
    out = json.loads(capsys.readouterr().out)
    assert out == {"scored": 42, "avg_M": 61.5, "classification": [
        {"code": "A", "label": "Strong", "n": 9}, {"code": "B", "label": "Weak", "n": 2}]}
    assert log[0] == ("connect", "/tmp/y.duckdb") and log[1] == "build" and log[-1] == "closed"
    sqls = [x for x in log if isinstance(x, tuple) and len(x) == 2 and isinstance(x[0], str) and x[0].startswith("SELECT")]
    assert [p for _, p in sqls] == [["JP"], ["JP"]]
    assert sqls[0][0] == "SELECT count(*) scored, round(avg(M),1) avg_M FROM dvm_global WHERE market = ?"
    assert sqls[1][0] == "SELECT code, label, count(*) n FROM dvm_composite WHERE market = ? GROUP BY 1,2 ORDER BY n DESC"
    assert sys.path[0] == "/repo" and wh.SEED == os.path.expanduser("~/market-data-artifacts/seed_ohlc")
    sys.path[:] = saved


def test_runner_unknown_mode_exits(monkeypatch, fake_stack):
    saved = list(sys.path)
    with pytest.raises(SystemExit) as e:
        execute("bench_python_runner.py", monkeypatch, ["--bogus"])
    assert "unknown mode --bogus" in str(e.value.code)
    sys.path[:] = saved


def test_runner_default_duckdb_path(monkeypatch, capsys, fake_stack):
    log, wh = fake_stack
    monkeypatch.delenv("BAZAARTALKS_BENCH_DUCKDB", raising=False)
    saved = list(sys.path)
    execute("bench_python_runner.py", monkeypatch, ["--process"])
    assert wh.DB == "/tmp/bazaartalks_bench/py_bench.duckdb"
    sys.path[:] = saved
