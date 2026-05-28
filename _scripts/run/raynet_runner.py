"""
This script will run the specified .ini using RayNet and the provided RL-driven protocol (e.g. Orca, Astrea, CleanSlate).
- The provided protocol name must be registered in this script's runner_paths dictionary
- The .ini MUST contain a Broker and a list of NED sources (ned-paths=...) for the simulation to run.
- A .ini configuration section can optionally be provided as the 3rd argument. Will default to "General" otherwise.

Usage: python raynet_runner.py <protocol> <ini_path> <section>
Example: ~/raynet/.venv/bin/python ~/raynet/_scripts/raynet_runner.py Orca ~/raynet/_experiments/responsiveness/responsiveness.ini Orca
"""

import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path

raynet_home = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(raynet_home))

from raynet_paths import materialize_raynet_ini

python = str(raynet_home / ".venv" / "bin" / "python") # RayNet's .venv (you can use your own if you have Ray/RLlib and other critical RL libraries)
runner_paths = {
    "orca": str(raynet_home / "simlibs" / "Orca" / "src" / "OrcaEval.py"),
    "cubic": str(raynet_home / "simlibs" / "Orca" / "src" / "CubicEval.py"),
    "astrea": str(raynet_home / "simlibs" / "Astrea" / "src" / "AstreaEval.py"),
    "cleanslate": str(raynet_home / "simlibs" / "CleanSlate" / "src" / "CleanSlateEval.py"),
}


def is_verbose() -> bool:
    return os.environ.get("RAYNET_VERBOSE", "").lower() in {"1", "true", "yes", "on"}


def logs_enabled() -> bool:
    return os.environ.get("RAYNET_LOG_OUTPUT", "1").lower() not in {"0", "false", "no", "off", "none"}


def log_path_for(protocol: str, ini_path: str, section: str) -> Path:
    log_dir = raynet_home / "_logs" / "raynet_runner"
    log_dir.mkdir(parents=True, exist_ok=True)
    name = f"{Path(ini_path).stem}-{section}-{protocol}-{datetime.now().strftime('%Y%m%d-%H%M%S')}-{os.getpid()}"
    safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "_", name).strip("._")
    return log_dir / f"{safe_name}.log"


def print_log_tail(log_path: Path, lines: int = 80) -> None:
    try:
        content = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return
    print(f"RayNet: last {min(lines, len(content))} log lines from {log_path}:")
    for line in content[-lines:]:
        print(line)

def get_registered_protocols():
    return list(runner_paths.keys())

def is_protocol_registered(protocol:str) -> bool:
    return protocol in runner_paths

def run_simulation(protocol:str, ini_path:str, section:str="General"):
    section = section or "General"
    if not is_protocol_registered(protocol):
        print(f"Error: Protocol '{protocol}' not recognized. Available protocols: {get_registered_protocols()}")
        sys.exit(1)
    runner = runner_paths[protocol]                           # The python script that uses Ray/RLlib to facilitate training/inference of the given protocol
    prepared_ini_path = materialize_raynet_ini(ini_path)
    log_path = log_path_for(protocol, prepared_ini_path, section)
    
    print("----------------------------------------")
    print(f"RayNet: Running protocol {protocol} \n\tRunner: {runner} \n\t.ini file: {prepared_ini_path} \n\tSection: {section}")
    if not is_verbose() and logs_enabled():
        print(f"\tLog: {log_path}")
    elif not is_verbose():
        print("\tLog: disabled")
    print("----------------------------------------")
    if is_verbose():
        subprocess.run([python, runner, prepared_ini_path, section], check=True)
        return

    if not logs_enabled():
        result = subprocess.run(
            [python, runner, prepared_ini_path, section],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode != 0:
            print("RayNet: simulation failed. Re-run with RAYNET_LOG_OUTPUT=1 or RAYNET_VERBOSE=1 for details.")
            sys.exit(result.returncode)
        return

    with log_path.open("w", encoding="utf-8") as log_file:
        result = subprocess.run(
            [python, runner, prepared_ini_path, section],
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        if result.returncode != 0:
            print_log_tail(log_path)
            sys.exit(result.returncode)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python raynet_runner.py <protocol> <ini_path> <section>")
        print("Example: python raynet_runner.py Orca ~/raynet/_experiments/responsiveness/responsiveness.ini Orca")
        sys.exit(1)
    params = {
        "protocol": sys.argv[1],                                      # Which RL protocol to run (e.g. Orca, Cubic, Astrea, CleanSlate)
        "ini_path": sys.argv[2],                                      # Path to the .ini file that specifies the simulation configuration to run.
        "section": sys.argv[3] if len(sys.argv) > 3 else None,        # Name of the .ini configuration section to run. "General", by default.
              }

    run_simulation(params['protocol'], params['ini_path'], params['section'])
