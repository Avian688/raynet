"""
This script will run the specified .ini using RayNet and the provided RL-driven protocol (e.g. Orca, Astrea, CleanSlate).
- The provided protocol name must be registered in this script's runner_paths dictionary
- The .ini MUST contain a Broker and a list of NED sources (ned-paths=...) for the simulation to run.
- A .ini configuration section can optionally be provided as the 3rd argument. Will default to "General" otherwise.

Usage: python raynet_runner.py <protocol> <ini_path> <section>
Example: ~/raynet/.venv/bin/python ~/raynet/_scripts/raynet_runner.py Orca ~/raynet/_experiments/responsiveness/responsiveness.ini Orca
"""

import os, sys
python = f"{os.getenv('HOME')}/raynet/.venv/bin/python" # RayNet's .venv (you can use your own if you have Ray/RLlib and other critical RL libraries)
runner_paths = {
    "orca": f"{os.getenv('HOME')}/raynet/simlibs/Orca/src/OrcaEval.py",
    "cubic": f"{os.getenv('HOME')}/raynet/simlibs/Orca/src/CubicEval.py",
    "astrea": f"{os.getenv('HOME')}/raynet/simlibs/Astrea/src/AstreaEval.py",
    "cleanslate": f"{os.getenv('HOME')}/raynet/simlibs/CleanSlate/src/CleanSlateEval.py",
}

def get_registered_protocols():
    return list(runner_paths.keys())

def is_protocol_registered(protocol:str) -> bool:
    return protocol in runner_paths

def run_simulation(protocol:str, ini_path:str, section:str="General"):
    if not is_protocol_registered(protocol):
        print(f"Error: Protocol '{protocol}' not recognized. Available protocols: {get_registered_protocols()}")
        sys.exit(1)
    runner = runner_paths[protocol]                           # The python script that uses Ray/RLlib to facilitate training/inference of the given protocol
    
    print("----------------------------------------")
    print(f"RayNet: Running protocol {protocol} \n\tRunner: {runner} \n\t.ini file: {ini_path} \n\tSection: {section}")
    print("----------------------------------------")
    os.system(f"{python} {runner} {ini_path} {section}")

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