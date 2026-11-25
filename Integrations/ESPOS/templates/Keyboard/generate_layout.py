"""Compatibility entry point into the project's pinned AdvancedKeyboard tools."""
import importlib.util
import runpy
from pathlib import Path

project = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("advanced_keyboard_dependency", project / "tools/setup_advanced_keyboard.py")
dependency = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dependency)
root = dependency.resolve_root(project)
runpy.run_path(str(root / "tools/layout/generate_layout.py"), run_name="__main__")
