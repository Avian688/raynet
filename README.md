Located at: https://github.com/CJUKnowles/raynet

# RayNet - RL Training Platform for Network Protocols
RayNet (Raynet was originally created by Luca Giacomoni in 2023) is a platform that enables the development of RL-driven congestion control protocols via the OMNeT++ discrete event simulator. This repo is an extension of RayNet designed to improve the user experience and expand RayNet's capabilities as part of a final year project at the University of Sussex.

## Requirements
This repository contains RayNet's source code and some scripts to train and evauate RL models. 

The system integrates the core Omnet++ discrete event simulator with its linked simulation libraries and Ray/RLlib through pybindings11. The figure below depicts the different packages/libraries in RayNet:

<img src="/docs/images/libraries.png" width="600">

Raynet requires (at least) the following third party (open-source) software:
- **Omnet++**: Provides the underlying simulation framework. This fork adds partial support for version 6.3 that is unfinished, but mostly functional. Alternatively, version 6.0 is more stable but may not work with newer versions of external dependencies like INET4.5.
- **INET**: Provides useful simulation components relating to computer networks and congestion control. A custom version of INET4.5 was used for this project (https://github.com/Avian688/inet4.5) and is required for Orca, Astrea, CleanSlate, TcpPaced, and Cubic. Several other simulation libraries by the same author were also utilized, including TcpPaced, Cubic, and more. These are assumed to be installed at `~/omnetpp/samples/`.
- **Ray/RLlib**: RayNet supports all traditional RL workflows by exposing a flexible simulation control API `OmnetBindApi`. However, Ray/RLlib is trivially supported and the recommended option for RayNet.
- **Python Modules:** Critical python modules like TensorFlow and PyTorch support training and evaluation scripts. A `requirements.txt` is provided that lists the essential modules, and `requirements-extra.txt` additionally provides **all** modules used in production of the final year project.

The build can use several local OMNeT++ installs. By default, `build.sh` selects the newest real `~/omnetpp-*` directory and then looks for INET under that install's `samples/inet*` directory. You can override those choices with `OMNETPP_ROOT`, `INET_ROOT`, the `build.sh` flags documented below, or a local `.raynet-build.env` file.

## Important RayNet Directories
- **src**: contains the binding API and a environment interface inspired by OMNeT++'s `cmdenv`. The contents of this directory collectively make up the simulation wrapper and will be compiled in to the `build` directory.

- **RLComponents**: contains critical simulation components like the `Broker` in addition to some helper classes.

- **simlibs**: contains various user-provided simulation libraries. This includes RL-driven CC schemes like `Orca` and `Astraea` as well as generally useful OMNeT++ components like `tcpPacedNoCC`. Users are encouraged to add any custom components here.

## FYP Evaluation Directories
As part of the final year project submission, several evaluation directories were created to support general experimentation. Users are welcome to use these as examples for plotting experiment results, but they are mostly fit-to-purpose and not intended to be generally useful.
- **_experiments**: Contains configuartion and scenario files to support experimentation.
- **_plots**: Evaluation scripts will automatically output aggregate plots here.
- **_results**: The experiment runner will parse simulation vector outputs, compile them into `.csv` files, and save them to this directory.
- **_scripts**: Used for various python scripts. By default, this contains an experiment runner and plotting script.
- **_topologies**: Intended to contain generally useful topologies to be shared among many experiments and training environments. Currently only contains a dumbbell topology.

These directories will likely be refactored after marking is complete.

## Building instrutions

Once the required dependencies (OMNet++ and INET) are installed, RayNet is ready to be built.

### Step 1 - Clone this repo
Clone this repository and its submodules
```
git clone --recurse-submodules -j8 
```

### Step 2 - Run the build script

Navigate to the RayNet directory and run the build script, which will automatically compile and link RayNet to OMNeT++, INET, and any simulation libraries contained in `raynet/simlibs`. 
```
cd ~/raynet
./build.sh
```
This process should be repeated any time you make changes to C++ code within the project or its simulation libraries. A few optional flags were provided for convenience, users are encouraged to explore `./build.sh`.

If auto-detection picks the wrong install, choose paths explicitly:
```
./build.sh -o ~/omnetpp-6.3.0 -i ~/omnetpp-6.3.0/samples/inet4.5
```
The same paths can be supplied via environment variables such as `OMNETPP_ROOT`, `INET_ROOT`, `TCPPACED_ROOT`, and `CUBIC_ROOT`.

For setups where OMNeT++ is not under `$HOME`, copy `.raynet-build.env.example` to `.raynet-build.env` and put the absolute paths there. `build.sh` sources that file automatically, and `.raynet-build.env` is ignored by git:
```
OMNETPP_ROOT=/its/home/av288/harddrive/omnetpp-6.1
OMNETPP_SAMPLES_ROOT=$OMNETPP_ROOT/samples
INET_ROOT=$OMNETPP_SAMPLES_ROOT/inet4.5
```

### Step 3 - Python environment

A Python environment with the modules specified in `requirements.txt` must be created prior to running RayNet. This may be the same environment used by your OMNeT++ installation if you wish, but a dedicated environment is recommended to avoid version conflicts:
```
./create-venv.sh
source .venv/bin/activate
```
`requirements-extra.txt` contains extra optional modules and specific versions if needed. Install that larger pinned environment with `./create-venv.sh --extra`; CUDA/NVIDIA packages are skipped automatically on macOS.
On Apple Silicon Macs, RayNet's Python architecture must match the OMNeT++/`omnetbind` architecture. For a Rosetta/x86_64 OMNeT++ install, use `./create-venv.sh --recreate --arch x86_64`; this selects the compatible macOS x86_64 Ray pin. For native arm64 OMNeT++, use `./create-venv.sh --recreate --arch arm64`.

## Usage
The easiest way to use RayNet is with the provided runner script `raynet/_scripts/run/raynet_runner.py`, which can use a trained model to perform inference on any RayNet-ready `.ini`.

Runner Usage:
```
python raynet_runner.py <protocol> <ini_path> <section>
```

Any simulation you wish to run must contain a Broker and a list of NED sources. More details provided within the runner script.

## Creating your own protocols
If you wish to implement and train your own schemes, it is recommended place them in `raynet/simlibs/` to be automatically compiled into the project via `./build.sh`. Just make sure it contains a valid `Makefile` similar to the provided simlib examples.

Orca and CleanSlate 

Refer to the Orca simlib for general usage examples. This contains examples of almost everything you will need, including a RayNet agent `Orca.cc`, a training script `OrcaTraining.py`, and an evaluation script `OrcaEval.py`.


[![DOI](https://zenodo.org/badge/561974777.svg)](https://zenodo.org/badge/latestdoi/561974777)
