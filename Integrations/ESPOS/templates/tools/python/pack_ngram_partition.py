"""Compatibility CLI/imports for the pinned AdvancedKeyboard NGPK packer."""
import importlib.util
from pathlib import Path

_project = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("advanced_keyboard_dependency", _project / "tools/setup_advanced_keyboard.py")
_dependency = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_dependency)
_source = _dependency.resolve_root(_project) / "tools/models/pack_ngram_partition.py"
__file__ = str(_source)
exec(compile(_source.read_bytes(), __file__, "exec"), globals())
