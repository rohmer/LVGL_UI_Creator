"""Evaluate generated host linker options without requiring a cross compiler."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from make_component import component_cmake


@unittest.skipUnless(shutil.which("cmake"), "CMake is required to evaluate generated target options")
class ComponentLinkerTests(unittest.TestCase):
    def link_options(self, *, apple=False, msvc=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            component = root / "component"
            (component / "cmake").mkdir(parents=True)
            (component / "tools").mkdir()
            (component / "tools/verify_inputs.py").write_text("# No project payload is needed by this target-options fixture.\n", encoding="utf-8")
            (component / "cmake/project_sources.cmake").write_text("", encoding="utf-8")
            (component / "features.json").write_text("{}", encoding="utf-8")
            (component / "CMakeLists.txt").write_text(component_cmake(), encoding="utf-8")
            # Keep CMake's target_link_options implementation active and inspect
            # its resulting property. Only the unrelated compilation setup is
            # stubbed, so all platforms can evaluate the Darwin/ELF/MSVC choice.
            wrapper = r'''cmake_minimum_required(VERSION 3.24)
project(ComponentLinkOptions NONE)
set(Python3_EXECUTABLE [[@PYTHON@]])
set(APPLE @APPLE@)
set(MSVC @MSVC@)
function(project)
endfunction()
function(add_library name)
    if(ARGV1 STREQUAL "ALIAS")
        _add_library(${ARGV})
    else()
        _add_library(${name} INTERFACE)
    endif()
endfunction()
function(target_compile_features)
endfunction()
function(target_compile_definitions)
endfunction()
function(target_include_directories)
endfunction()
function(target_compile_options)
endfunction()
function(set_target_properties)
endfunction()
add_subdirectory(component)
get_target_property(options lvgl_pb_esp32 INTERFACE_LINK_OPTIONS)
if(NOT options)
    set(options "")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/link-options.txt" "${options}")
'''
            wrapper = wrapper.replace("@PYTHON@", Path(sys.executable).as_posix())
            wrapper = wrapper.replace("@APPLE@", "ON" if apple else "OFF")
            wrapper = wrapper.replace("@MSVC@", "ON" if msvc else "OFF")
            (root / "CMakeLists.txt").write_text(wrapper, encoding="utf-8")
            command = [shutil.which("cmake"), "-S", str(root), "-B", str(root / "build")]
            if shutil.which("ninja"):
                command += ["-G", "Ninja"]
            result = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            options = (root / "build/link-options.txt").read_text(encoding="utf-8")
            return options.split(";") if options else []

    def test_macos_uses_darwin_dead_strip(self):
        self.assertEqual(self.link_options(apple=True), ["-Wl,-dead_strip"])

    def test_elf_retains_section_garbage_collection(self):
        self.assertEqual(self.link_options(), ["-Wl,--gc-sections"])

    def test_msvc_does_not_receive_unix_linker_options(self):
        self.assertEqual(self.link_options(msvc=True), [])


if __name__ == "__main__":
    unittest.main()
