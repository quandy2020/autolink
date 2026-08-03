#!/usr/bin/env python3

import os
import sys
from pathlib import Path


def _append_if_exists(path: Path) -> None:
    if path.exists():
        path_str = str(path)
        if path_str not in sys.path:
            sys.path.insert(0, path_str)


def setup_autolink_pythonpath() -> None:
    """Bootstrap PYTHONPATH for `import autolink` (build/python or install)."""
    cwd = Path.cwd()
    repo_root = cwd
    for _ in range(8):
        if (repo_root / "autolink" / "python").exists() or (
                repo_root / "build" / "python").exists():
            break
        if repo_root.parent == repo_root:
            break
        repo_root = repo_root.parent

    build_dir = Path(os.environ.get("AUTOLINK_BUILD_DIR", repo_root / "build"))
    _append_if_exists(build_dir / "python")

    home = Path(os.environ.get("AUTOLINK_DISTRIBUTION_HOME", "/usr/local"))
    _append_if_exists(home / "python")

    # Walk up from examples/python → repo root variants.
    _append_if_exists(repo_root / "build" / "python")
    _append_if_exists(repo_root / "install" / "python")
    _append_if_exists(cwd / "build" / "python")
    _append_if_exists(cwd.parent.parent / "build" / "python")
