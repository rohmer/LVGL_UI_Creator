"""Resolve the pinned AdvancedKeyboard source for runtime/profile exporters."""
from functools import lru_cache
import importlib.util
from pathlib import Path, PurePosixPath
import shutil

PREFIX = "3rdParty/AdvancedKeyboard"


@lru_cache(maxsize=4)
def integration(project_root: Path):
    path = Path(project_root).resolve() / "Integrations/AdvancedKeyboard/dependency.py"
    if not path.is_file():
        raise ValueError("The AdvancedKeyboard dependency resolver is missing from this source toolkit")
    spec = importlib.util.spec_from_file_location("uicreator_keyboard_dependency", path)
    if not spec or not spec.loader:
        raise ValueError("Cannot load the AdvancedKeyboard dependency resolver")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if module.DEFAULT_CREATOR_PATH != PREFIX:
        raise ValueError("The AdvancedKeyboard dependency path differs from this generator's source layout")
    return module


def resolve_root(project_root: Path) -> Path:
    return Path(integration(Path(project_root)).resolve_root(Path(project_root)))


def source_file(project_root: Path, relative: str) -> Path:
    path = PurePosixPath(relative)
    prefix = PurePosixPath(PREFIX)
    if path.is_relative_to(prefix):
        return resolve_root(project_root).joinpath(*path.relative_to(prefix).parts)
    return Path(project_root) / relative


def copy_runtime(project_root: Path, destination: Path):
    module = integration(Path(project_root))
    module.copy_runtime(resolve_root(project_root), Path(destination))


def copy_integration(project_root: Path, destination: Path):
    """Keep an offline toolkit's resolver and pin metadata beside its runtime."""
    source = Path(project_root) / "Integrations/AdvancedKeyboard"
    target = Path(destination) / "Integrations/AdvancedKeyboard"
    target.mkdir(parents=True, exist_ok=True)
    for path in source.glob("dependency*"):
        if path.is_file() and path.suffix in (".py", ".json"):
            shutil.copy2(path, target / path.name)
    lock = Path(project_root) / integration(Path(project_root)).LOCK_NAME
    if lock.is_file():
        shutil.copy2(lock, Path(destination) / lock.name)
    cmake = Path(project_root) / "cmake/AdvancedKeyboardDependency.cmake"
    if cmake.is_file():
        target = Path(destination) / "cmake"
        target.mkdir(parents=True, exist_ok=True)
        shutil.copy2(cmake, target / cmake.name)
