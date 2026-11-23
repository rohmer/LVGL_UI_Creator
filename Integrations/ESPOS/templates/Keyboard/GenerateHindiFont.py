"""Compatibility entry point; maintain shared font generation in AdvancedKeyboard."""
import runpy
from pathlib import Path

runpy.run_path(str(Path(__file__).resolve().parent / "AdvancedKeyboard" / "GenerateHindiFont.py"), run_name="__main__")
