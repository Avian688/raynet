"""Path helpers for running RayNet from different checkout locations."""

from __future__ import annotations

import os
import re
from pathlib import Path

_ABSOLUTE_RAYNET_ROOT = re.compile(
    r"(?<![\w.-])(?:/[^\s:;\"']+)+/raynet(?=$|[/:;\s\"'])"
)
_ABSOLUTE_INET_ROOT = re.compile(
    r"(?<![\w.-])(?:/[^\s:;\"']+)+/inet(?:\d|[.-])[^/:;\s\"']*(?=$|[/:;\s\"'])"
)


def raynet_home() -> Path:
    configured = os.environ.get("RAYNET_HOME")
    if configured:
        configured_path = Path(configured).expanduser().resolve(strict=False)
        if configured_path.is_dir():
            return configured_path
    return Path(__file__).resolve().parent


def _load_build_config() -> dict[str, str]:
    config_path = raynet_home() / ".raynet-build.env"
    if not config_path.is_file():
        return {}

    values: dict[str, str] = {}
    env = dict(os.environ)
    for raw_line in config_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip().removeprefix("export ").strip()
        if not key:
            continue
        value = value.strip().strip("'\"")
        expanded = os.path.expanduser(os.path.expandvars(value))
        for name, replacement in {**env, **values}.items():
            expanded = expanded.replace(f"${name}", replacement)
            expanded = expanded.replace(f"${{{name}}}", replacement)
        values[key] = expanded
    return values


def _configured_path(name: str) -> str | None:
    config = _load_build_config()
    value = os.environ.get(name) or config.get(name)
    if value:
        return str(Path(value).expanduser().resolve(strict=False))
    return None


def omnetpp_root() -> Path | None:
    configured = _configured_path("OMNETPP_ROOT")
    return Path(configured) if configured else None


def omnetpp_samples_root() -> Path | None:
    configured = _configured_path("OMNETPP_SAMPLES_ROOT")
    if configured:
        return Path(configured)
    root = omnetpp_root()
    return root / "samples" if root else None


def inet_root() -> Path | None:
    configured = _configured_path("INET_ROOT")
    if configured:
        return Path(configured)

    samples = omnetpp_samples_root()
    if not samples:
        return None
    candidates = sorted(path for path in samples.glob("inet*") if (path / "src").is_dir())
    return candidates[-1] if candidates else None


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
    text = _ABSOLUTE_RAYNET_ROOT.sub(root, text)

    samples = omnetpp_samples_root()
    if samples:
        samples_text = str(samples)
        for old in (
            "${OMNETPP_SAMPLES_ROOT}",
            "$OMNETPP_SAMPLES_ROOT",
            "OMNETPP_SAMPLES_ROOT",
            "${HOME}/omnetpp/samples",
            "$HOME/omnetpp/samples",
            "HOME/omnetpp/samples",
            "~/omnetpp/samples",
        ):
            text = text.replace(old, samples_text)

    omnet = omnetpp_root()
    if omnet:
        omnet_text = str(omnet)
        for old in (
            "${OMNETPP_ROOT}",
            "$OMNETPP_ROOT",
            "OMNETPP_ROOT",
            "${HOME}/omnetpp",
            "$HOME/omnetpp",
            "HOME/omnetpp",
            "~/omnetpp",
        ):
            text = text.replace(old, omnet_text)

    inet = inet_root()
    if inet:
        inet_text = str(inet)
        for old in (
            "${INET_ROOT}",
            "$INET_ROOT",
            "INET_ROOT",
            "${HOME}/inet4.5",
            "$HOME/inet4.5",
            "HOME/inet4.5",
            "~/inet4.5",
        ):
            text = text.replace(old, inet_text)
        text = _ABSOLUTE_INET_ROOT.sub(inet_text, text)

    return text


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
