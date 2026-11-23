import importlib.util
import pathlib

import pytest

REPO = pathlib.Path(__file__).resolve().parents[1]


@pytest.fixture
def cl(tmp_path, monkeypatch):
    spec = importlib.util.spec_from_file_location("check_layering_under_test", REPO / "scripts" / "check_layering.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    monkeypatch.setattr(mod, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(mod, "LIBS", tmp_path / "libs")
    monkeypatch.setattr(mod, "SERVICES", tmp_path / "services")
    monkeypatch.setattr(mod, "SIDECAR", tmp_path / "python-sidecar")
    return mod


def write(root, rel, text):
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)
    return p


def test_real_repo_passes_its_own_layering_check():
    spec = importlib.util.spec_from_file_location("cl_real", REPO / "scripts" / "check_layering.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    assert mod.violations() == []


def test_missing_directories_are_fine(cl):
    assert cl.cmake_files() == [] and cl.violations() == []


def test_clean_tree(cl, tmp_path):
    write(tmp_path, "libs/calendar/CMakeLists.txt", "add_library(calendar src/a.cpp)\n")
    write(tmp_path, "libs/calendar/src/a.cpp", '#include "bazaartalks/calendar/a.hpp"\n')
    write(tmp_path, "services/cli/CMakeLists.txt", "target_link_libraries(cli PRIVATE calendar)\n")
    assert cl.violations() == []


def test_libs_cmake_referencing_services_is_flagged(cl, tmp_path):
    write(tmp_path, "libs/a/CMakeLists.txt", "target_link_libraries(a PRIVATE services/foo)\n")
    write(tmp_path, "libs/b/CMakeLists.txt", "target_link_libraries(b PRIVATE bar_services_x services)\n")   # bare word `services`
    write(tmp_path, "libs/c/CMakeLists.txt", "# the microservices layer\ntarget_link_libraries(c PRIVATE d)\n")   # `microservices` is not the word
    errs = cl.violations()
    assert len(errs) == 2
    assert any(e.startswith("libs/a/CMakeLists.txt: libs/ target references services/") for e in errs)
    assert any(e.startswith("libs/b/CMakeLists.txt:") for e in errs)
    assert not any("libs/c" in e for e in errs)


def test_sidecar_reference_in_cmake_or_sources_is_flagged(cl, tmp_path):
    write(tmp_path, "libs/x/CMakeLists.txt", "add_subdirectory(../../python-sidecar)\n")
    write(tmp_path, "libs/x/src/x.cpp", '#include "python-sidecar/api.h"\n')
    write(tmp_path, "libs/x/include/x.hpp", "// see python-sidecar/\n")
    write(tmp_path, "services/s/CMakeLists.txt", "include(python-sidecar/cmake)\n")
    write(tmp_path, "services/s/main.cpp", "int main(){}\n")
    write(tmp_path, "services/s/notes.txt", "python-sidecar mentioned in a .txt is allowed\n")
    errs = cl.violations()
    flagged = sorted(e.split(":")[0] for e in errs if "references python-sidecar/" in e)
    assert flagged == ["libs/x/CMakeLists.txt", "libs/x/include/x.hpp", "libs/x/src/x.cpp", "services/s/CMakeLists.txt"]
    assert not any("notes.txt" in e for e in errs)


def test_services_may_reference_services_in_cmake(cl, tmp_path):
    write(tmp_path, "services/a/CMakeLists.txt", "target_link_libraries(a PRIVATE services_common)\n")
    assert cl.violations() == []


def test_non_utf8_source_is_tolerated(cl, tmp_path):
    p = tmp_path / "libs" / "x" / "src"
    p.mkdir(parents=True)
    (p / "bad.cpp").write_bytes(b"\xff\xfe python-sidecar \xff")
    assert any("bad.cpp" in e for e in cl.violations())


def test_main_exit_codes_and_messages(cl, tmp_path, capsys):
    assert cl.main() == 0
    assert capsys.readouterr().out.startswith("Layering check passed")
    write(tmp_path, "libs/a/CMakeLists.txt", "services\n")
    assert cl.main() == 1
    cap = capsys.readouterr()
    assert cap.out == "" and cap.err.startswith("Layering violations found:") and "  - libs/a/CMakeLists.txt" in cap.err


def test_entry_point_exits_with_main_status(tmp_path, monkeypatch):
    import runpy
    with pytest.raises(SystemExit) as e:
        runpy.run_path(str(REPO / "scripts" / "check_layering.py"), run_name="__main__")
    assert e.value.code == 0
