"""Compatibility helpers for RayNet experiment checkpoints."""

from __future__ import annotations

import importlib
import sys
from typing import Any


def install_numpy_core_aliases() -> None:
    """Allow NumPy 2.x pickles to load under NumPy 1.x.

    Some checkpoints refer to private NumPy 2 modules such as
    ``numpy._core.numeric``. RayNet's Rosetta/x86_64 environment uses
    NumPy 1.26 for PyTorch compatibility, where the equivalent modules
    live under ``numpy.core``.
    """
    try:
        importlib.import_module("numpy._core.numeric")
        return
    except ModuleNotFoundError:
        pass

    try:
        numpy_core = importlib.import_module("numpy.core")
    except ModuleNotFoundError:
        return

    sys.modules.setdefault("numpy._core", numpy_core)
    for module_name in (
        "_exceptions",
        "_internal",
        "_methods",
        "_multiarray_umath",
        "arrayprint",
        "defchararray",
        "einsumfunc",
        "fromnumeric",
        "function_base",
        "getlimits",
        "multiarray",
        "numeric",
        "numerictypes",
        "records",
        "shape_base",
        "umath",
    ):
        try:
            module = importlib.import_module(f"numpy.core.{module_name}")
        except ModuleNotFoundError:
            continue
        sys.modules.setdefault(f"numpy._core.{module_name}", module)


def install_rllib_checkpoint_compat() -> None:
    """Allow Ray 2.53 checkpoints to restore under Ray 2.49 on macOS x86_64.

    Ray 2.53 writes metrics states with a newer schema. The newest Ray wheel
    available for macOS x86_64 is currently 2.49.2, whose metrics restore code
    expects the older schema and raises before policy weights finish loading.
    Metrics are bookkeeping; for RayNet inference we can keep the freshly
    created metric loggers and still restore the trained model state.
    """
    install_numpy_core_aliases()

    try:
        from ray.rllib.utils.metrics.metrics_logger import MetricsLogger
    except Exception:
        return

    if getattr(MetricsLogger.set_state, "_raynet_checkpoint_compat", False):
        return

    original_set_state = MetricsLogger.set_state

    def set_state_compat(self: Any, state: dict[str, Any]) -> None:
        try:
            original_set_state(self, state)
        except (KeyError, TypeError, IndexError):
            if _looks_like_new_rllib_metrics_state(state):
                return
            raise

    set_state_compat._raynet_checkpoint_compat = True  # type: ignore[attr-defined]
    set_state_compat._raynet_original_set_state = original_set_state  # type: ignore[attr-defined]
    MetricsLogger.set_state = set_state_compat


def _looks_like_new_rllib_metrics_state(state: Any) -> bool:
    if not isinstance(state, dict):
        return False

    stats = state.get("stats")
    if not isinstance(stats, dict):
        return False

    for stats_state in stats.values():
        if not isinstance(stats_state, dict):
            continue
        if "stats_cls_identifier" not in stats_state:
            continue
        if "values" not in stats_state:
            return True
        if "reduce" not in stats_state or "_hist" not in stats_state:
            return True
    return False
