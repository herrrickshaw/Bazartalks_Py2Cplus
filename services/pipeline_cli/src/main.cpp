// Port of pipeline.py -- the end-to-end orchestrator: source -> validate
// -> process -> analyze -> graphics, over the platform's existing
// scripts/libraries. Matches the Python original's own architecture
// exactly: nothing here reimplements what already exists elsewhere.
//
// Stage-by-stage scope (mirrors Python's own mix of "call a native
// function" vs. "shell out to a script"):
//   --process / --analyze : NATIVE, using the already-ported Warehouse
//     (Phase 2/8) directly -- these were pure DuckDB SQL in the Python
//     original too (`import warehouse; warehouse.build(con); con.execute(...)`),
//     so there's a genuine C++ path now, unlike the other stages.
//   --source / --validate : shell out to the existing standalone Python
//     scanner/validator scripts (`full_{market}_market_scan.py`,
//     `data_quality.py`) via execvp (no shell, matching Python's
//     `subprocess.run([sys.executable, script, ...])` exactly -- not
//     `system()`, which would introduce shell-injection risk the
//     original never had).
//   --graphics : shells out to dashboard.py's OWN CLI (`--out <path>`),
//     which already exists standalone -- dashboard.py's render_html()
//     itself was explicitly deferred in Phase 8 (depends on
//     pandas.DataFrame.to_html()'s exact markup), so there is no native
//     path to call into yet.
//   --live : has NO standalone script to delegate to in the Python
//     original (it's inlined in pipeline.py's own stage_live(), calling
//     live_fundamentals.fetch_batch() -- Trendlyne/Screener.in scraping,
//     permanently Python-sidecar, no portable structure at all) -- this
//     port delegates to the ORIGINAL pipeline.py's `--live` flag directly
//     rather than inventing a new intermediate script that doesn't exist.
//     This is a deliberate, documented fallback, not a silently-missing
//     feature.
//
// This file itself (argv parsing, subprocess plumbing, stdout summary) is
// data-plumbing, untested here by design -- matching every other phase's
// "main() is out of scope" boundary. What's native (Warehouse::build()/
// query()) is already tested in libs/storage/duckdb_client.
#include "bazaartalks/storage/warehouse.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

using bazaartalks::storage::Warehouse;
using bazaartalks::storage::WarehousePaths;

namespace {

const std::map<std::string, std::string>& market_scripts() {
  static const std::map<std::string, std::string> kScripts = {
      {"us", "full_us_market_scan.py"},
      {"indian", "full_indian_market_scan.py"},
      {"japan", "full_japan_market_scan.py"},
      {"korea", "full_korea_market_scan.py"},
      {"european", "full_european_market_scan.py"},
  };
  return kScripts;
}

std::string g_duckdb_path = "market.duckdb";
std::string g_sidecar_dir = ".";
WarehousePaths g_paths;

bool is_safe_market_code(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isalnum(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

// Port of `subprocess.run([sys.executable, script, *extra_args])`:
// execvp (no shell), wait, and report whether the exit code was 0,
// matching `r.returncode == 0`.
bool run_python_script(const std::string& script, const std::vector<std::string>& extra_args) {
  std::string path = g_sidecar_dir + "/" + script;
  std::vector<std::string> argv_storage = {"python3", path};
  for (const auto& a : extra_args) argv_storage.push_back(a);

  std::vector<char*> exec_argv;
  for (auto& s : argv_storage) exec_argv.push_back(s.data());
  exec_argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "  [pipeline] fork failed for " << script << std::endl;
    return false;
  }
  if (pid == 0) {
    execvp("python3", exec_argv.data());
    _exit(127);  // execvp only returns on failure
  }
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool stage_source(const std::string& market) {
  const auto& scripts = market_scripts();
  auto it = scripts.find(market);
  if (it == scripts.end()) {
    std::cerr << "  [source] unknown market '" << market << "'" << std::endl;
    return false;
  }
  std::cout << "  [source] running " << it->second << " ..." << std::endl;
  return run_python_script(it->second, {});
}

bool stage_validate() {
  std::cout << "  [validate] running data_quality.py ..." << std::endl;
  return run_python_script("data_quality.py", {});
}

bool stage_process() {
  std::cout << "  [process] rebuilding warehouse views ..." << std::endl;
  Warehouse wh(g_duckdb_path, g_paths);
  wh.build();
  auto tables = wh.tables();
  std::cout << "  [process] views ready: [";
  for (std::size_t i = 0; i < tables.size(); ++i) {
    if (i) std::cout << ", ";
    std::cout << tables[i];
  }
  std::cout << "]" << std::endl;
  return true;
}

bool stage_analyze(const std::string& market) {
  std::cout << "  [analyze] " << market << ": DVM + accumulation screen ..." << std::endl;
  if (!is_safe_market_code(market)) {
    std::cerr << "  [analyze] invalid market code" << std::endl;
    return false;
  }
  Warehouse wh(g_duckdb_path, g_paths);
  wh.build();
  auto tables = wh.tables();
  std::set<std::string> table_set(tables.begin(), tables.end());

  if (table_set.count("dvm_global")) {
    auto result = wh.query("SELECT count(*) scored, round(avg(M),1) avg_M FROM dvm_global "
                           "WHERE market = '" +
                           market + "'");
    std::cout << "  [analyze] DVM momentum (" << market << "):" << std::endl;
    for (auto& row : *result) {
      // avg_M is a DOUBLE (round(avg(M),1)) -- GetValue<std::string>() then
      // printing that string would round-trip through DuckDB's reduced-
      // precision string conversion for no reason (the value's already
      // rounded by the query itself, so it happens not to visibly truncate
      // here, but it's the same unsafe pattern the Phase 9 cutover found
      // silently discarding real precision in serve_api's routes -- fixed
      // for consistency, not because this specific spot was observed to
      // misbehave). AVG() over zero matching rows is NULL, not 0 -- print
      // that explicitly rather than crashing on GetValue<double>() of a
      // NULL value.
      std::cout << "    scored=" << row.GetValue<int64_t>(0) << " avg_M="
                << (row.IsNull(1) ? std::string("NULL") : std::to_string(row.GetValue<double>(1)))
                << std::endl;
    }
  } else {
    std::cout << "  [analyze] dvm_global not built locally -- skipping momentum summary"
              << std::endl;
  }

  if (table_set.count("dvm_composite")) {
    auto result = wh.query("SELECT code, label, count(*) n FROM dvm_composite WHERE market = '" +
                           market + "' GROUP BY 1,2 ORDER BY n DESC");
    std::cout << "  [analyze] screener.in-style classification (" << market << "):" << std::endl;
    for (auto& row : *result) {
      std::cout << "    code=" << row.GetValue<std::string>(0)
                << " label=" << row.GetValue<std::string>(1)
                << " n=" << row.GetValue<int64_t>(2) << std::endl;
    }
  } else {
    std::cout << "  [analyze] dvm_composite not built locally -- skipping classification summary"
              << std::endl;
  }

  // accumulation_screener.py's current_screen() is itself a full
  // Python-sidecar module (yfinance-backed OHLC panel scan, no C++ port),
  // so this stage only reports the DVM summaries natively; the
  // accumulation count line the Python original prints is not
  // reproduced here without shelling back out, which --graphics already
  // does for the equivalent dashboard section.
  return true;
}

bool stage_live(const std::string& market, std::optional<int> limit, double pause) {
  std::cout << "  [live] no standalone script exists for this stage in the Python "
            << "original (it's inlined Trendlyne/Screener.in scraping) -- delegating "
            << "to pipeline.py --live " << market << " directly" << std::endl;
  std::vector<std::string> args = {"--live", market, "--pause", std::to_string(pause)};
  if (limit) {
    args.push_back("--limit");
    args.push_back(std::to_string(*limit));
  }
  return run_python_script("pipeline.py", args);
}

bool stage_graphics(const std::string& market) {
  std::cout << "  [graphics] rendering dashboard for " << market << " ..." << std::endl;
  std::string out = g_sidecar_dir + "/pipeline_dashboard_" + market + ".html";
  return run_python_script("dashboard.py", {"--out", out});
}

struct Args {
  std::optional<std::string> source;
  bool validate = false;
  bool process = false;
  std::optional<std::string> analyze;
  std::optional<std::string> graphics;
  std::optional<std::string> live;
  std::optional<int> limit;
  double pause = 1.5;
  std::optional<std::string> all;
};

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
    if (arg == "--source") {
      a.source = next();
    } else if (arg == "--validate") {
      a.validate = true;
    } else if (arg == "--process") {
      a.process = true;
    } else if (arg == "--analyze") {
      a.analyze = next();
    } else if (arg == "--graphics") {
      a.graphics = next();
    } else if (arg == "--live") {
      a.live = next();
    } else if (arg == "--limit") {
      a.limit = std::stoi(next());
    } else if (arg == "--pause") {
      a.pause = std::stod(next());
    } else if (arg == "--all") {
      a.all = next();
    } else if (arg == "--duckdb") {
      g_duckdb_path = next();
    } else if (arg == "--sidecar-dir") {
      g_sidecar_dir = next();
    }
  }

  if (!a.source && !a.validate && !a.process && !a.analyze && !a.graphics && !a.live && !a.all) {
    std::cout << "usage: bt_pipeline_cli [--source MARKET] [--validate] [--process] "
                 "[--analyze MARKET] [--graphics MARKET] [--live MARKET] [--limit N] "
                 "[--pause SECONDS] [--all MARKET] [--duckdb PATH] [--sidecar-dir DIR]"
              << std::endl;
    return 0;
  }

  std::vector<std::pair<std::string, bool>> results;

  if (a.source) results.emplace_back("source", stage_source(*a.source));
  if (a.process) results.emplace_back("process", stage_process());
  if (a.validate) results.emplace_back("validate", stage_validate());
  if (a.analyze) results.emplace_back("analyze", stage_analyze(upper(*a.analyze)));
  if (a.live) results.emplace_back("live", stage_live(upper(*a.live), a.limit, a.pause));
  if (a.graphics) results.emplace_back("graphics", stage_graphics(upper(*a.graphics)));

  if (a.all) {
    std::string market = upper(*a.all);
    // Overwrites any earlier same-named entry, matching Python's dict
    // assignment `results["process"] = ...` semantics exactly.
    auto set_result = [&](const std::string& name, bool ok) {
      for (auto& [n, v] : results) {
        if (n == name) {
          v = ok;
          return;
        }
      }
      results.emplace_back(name, ok);
    };
    set_result("process", stage_process());
    set_result("validate", stage_validate());
    set_result("analyze", stage_analyze(market));
    set_result("graphics", stage_graphics(market));
  }

  std::cout << "\n=== PIPELINE SUMMARY ===" << std::endl;
  bool all_ok = true;
  for (const auto& [stage, ok] : results) {
    std::cout << "  " << stage << std::string(std::max<int>(0, 10 - (int)stage.size()), ' ')
              << " " << (ok ? "PASS" : "FAIL") << std::endl;
    all_ok = all_ok && ok;
  }
  return all_ok ? 0 : 1;
}
