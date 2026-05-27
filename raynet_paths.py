"""Path helpers for running RayNet from different checkout locations."""

from __future__ import annotations

import os
import re
from pathlib import Path

_ABSOLUTE_RAYNET_ROOT = re.compile(
    r"(?<![\w.-])(?:/home/[^/\s:;\"']+|/Users/[^/\s:;\"']+)/raynet(?=$|[/:;\s\"'])"
)


def raynet_home() -> Path:
    configured = os.environ.get("RAYNET_HOME")
    if configured:
        return Path(configured).expanduser().resolve()
    return Path(__file__).resolve().parent


def normalize_raynet_ini_text(text: str) -> str:
    """Rewrite RayNet paths in OMNeT++ ini content for this checkout."""
    root = str(raynet_home())
    for old in (
        "${RAYNET_HOME}",
        "$RAYNET_HOME",
        "RAYNET_HOME",
        "${HOME}/raynet",
        "$HOME/raynet",
        "HOME/raynet",
        "~/raynet",
    ):
        text = text.replace(old, root)
    return _ABSOLUTE_RAYNET_ROOT.sub(root, text)


def materialize_raynet_ini(ini_path: str) -> str:
    """Return an ini path whose RayNet paths match this machine.

    If the file already matches, the original path is returned. If not, a
    generated copy is written next to the source ini so existing relative NED
    paths keep the same meaning.
    """
    source = Path(ini_path).expanduser()
    if not source.is_absolute():
        source = Path.cwd() / source
    source = source.resolve(strict=False)

    ini_text = source.read_text(encoding="utf-8")
    normalized_text = normalize_raynet_ini_text(ini_text)
    if normalized_text == ini_text:
        return str(source)

    generated = source.with_name(f".{source.stem}.raynet.{os.getpid()}{source.suffix}")
    generated.write_text(normalized_text, encoding="utf-8")
    return str(generated)
