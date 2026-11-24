import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("macos_frontend_test", ROOT / "Packaging/BuildMacOS.py")
frontend = importlib.util.module_from_spec(spec)
spec.loader.exec_module(frontend)


def tools():
    architectures = {}
    for architecture, rust in (("x86_64", "x86_64"), ("arm64", "aarch64")):
        prefix = "/cache/toolchain/bin/" + architecture + "-apple-darwin24.4-"
        architectures[architecture] = {"cc": prefix + "clang", "cxx": prefix + "clang++",
            "objc": prefix + "clang", "objcxx": prefix + "clang++", "ar": prefix + "ar",
            "rustTarget": rust + "-apple-darwin"}
    return {"cmake": "/cache/cmake/bin/cmake", "deploymentTarget": "13.5",
        "osxcross": {"prefix": "/cache/toolchain", "target": "darwin24.4"},
        "sdk": {"version": "15.4", "path": "/cache/toolchain/SDK/MacOSX15.4.sdk"},
        "llvm": {"bin": "/usr/lib/llvm-21/bin"},
        "rust": {"version": "1.85.1", "cargo": "/cache/rust/cargo/bin/cargo",
                 "rustupHome": "/cache/rust/rustup", "cargoHome": "/cache/rust/cargo"},
        "architectures": architectures}


class MacOSCrossFrontendTests(unittest.TestCase):
    def test_c_and_objective_c_use_the_selected_target_sdk(self):
        for architecture in ("x86_64", "arm64"):
            with self.subTest(architecture=architecture):
                text = frontend.toolchain_text(tools(), architecture)
                self.assertIn('set(CMAKE_SYSTEM_NAME "Darwin")', text)
                self.assertIn('set(CMAKE_OSX_ARCHITECTURES "' + architecture + '")', text)
                self.assertIn('set(CMAKE_OSX_DEPLOYMENT_TARGET "13.5")', text)
                self.assertIn('MacOSX15.4.sdk', text)
                for compiler in ("C", "CXX", "OBJC", "OBJCXX"):
                    self.assertIn('set(CMAKE_' + compiler + '_COMPILER "/cache/toolchain/bin/' + architecture + '-apple-darwin', text)
                self.assertIn('set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM "NEVER")', text)
                self.assertIn('set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "ONLY")', text)

    def test_toolchain_rejects_unsupported_architectures_and_cmake_injection(self):
        with self.assertRaises(ValueError):
            frontend.toolchain_text(tools(), "i686")
        invalid = tools()
        invalid["sdk"]["path"] = '/cache/sdk");execute_process(COMMAND other)'
        with self.assertRaises(ValueError):
            frontend.toolchain_text(invalid, "arm64")

    def test_cross_environment_keeps_host_programs_and_target_compilers_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            parent = {"PATH": "/target-mac-node/bin:/old-linux-sysroot/bin", "PYTHONHOME": "/host-python",
                "CPATH": "/linux/include", "LIBRARY_PATH": "/linux/lib", "RUSTFLAGS": "-C link-arg=linux",
                "CARGO_BUILD_TARGET": "aarch64-unknown-linux-gnu", "PKG_CONFIG_PATH": "/linux/pkgconfig"}
            value = frontend.environment(tools(), "arm64", Path(directory), parent)
            for name in ("PYTHONHOME", "CPATH", "LIBRARY_PATH", "RUSTFLAGS", "CARGO_BUILD_TARGET", "PKG_CONFIG_PATH"):
                self.assertNotIn(name, value)
            self.assertNotIn("/target-mac-node", value["PATH"])
            self.assertNotIn("/cache/host-node/bin", value["PATH"])
            for host_tool in ("/cache/cmake/bin", "/cache/rust/cargo/bin", "/usr/lib/llvm-21/bin"):
                self.assertIn(host_tool, value["PATH"])
            self.assertEqual(value["CARGO_TARGET_AARCH64_APPLE_DARWIN_LINKER"], value["CC"])
            self.assertEqual(value["RUSTUP_TOOLCHAIN"], "1.85.1")
            self.assertEqual(value["SDKROOT"], tools()["sdk"]["path"])
            self.assertTrue(Path(value["PKG_CONFIG_LIBDIR"]).is_dir())

    def test_unsafe_release_cannot_escape_sdk_cache(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(frontend.urllib.request, "urlopen") as fetch:
            with self.assertRaises(ValueError):
                frontend.download_sdk_inputs(Path(directory), "../../outside")
            fetch.assert_not_called()
            self.assertFalse(list(Path(directory).iterdir()))

    def test_failed_sdk_checksum_preserves_previous_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            destination = base / "sdk-inputs/0.9.2"
            destination.mkdir(parents=True)
            archive = destination / "LVGLProtobuf-ESP32-Toolkit.zip"
            archive.write_bytes(b"existing cache")
            manifest = {"version": "0.9.2", "packages": [{"file": archive.name, "bytes": 3, "sha256": "0" * 64}]}
            with mock.patch.object(frontend, "SDK_ARCHIVES", (archive.name,)), \
                 mock.patch.object(frontend.urllib.request, "urlopen", side_effect=[io.BytesIO(json.dumps(manifest).encode()), io.BytesIO(b"bad")]):
                with self.assertRaises(ValueError):
                    frontend.download_sdk_inputs(base, "0.9.2")
            self.assertEqual(archive.read_bytes(), b"existing cache")
            self.assertFalse(list(destination.glob("*.download")))


if __name__ == "__main__":
    unittest.main()
