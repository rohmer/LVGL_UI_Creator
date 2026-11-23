"""Compatibility entry point; maintain shared layout generation in AdvancedKeyboard."""
import runpy
from pathlib import Path

runpy.run_path(str(Path(__file__).resolve().parent / "AdvancedKeyboard" / "layout" / "generate_layout.py"), run_name="__main__")
