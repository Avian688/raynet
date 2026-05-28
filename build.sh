#!/bin/bash
set -e

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_config="${RAYNET_BUILD_CONFIG:-$script_dir/.raynet-build.env}"
if [ -f "$build_config" ]; then
    source "$build_config"
fi
export RAYNET_HOME="${RAYNET_HOME:-$script_dir}"
host_system="$(uname -s)"
host_machine="$(uname -m)"
if [ "$host_system" = "Darwin" ]; then
    if native_machine=$(arch -arm64 /usr/bin/uname -m 2>/dev/null) && [ "$native_machine" = "arm64" ]; then
        host_machine=arm64
    fi
fi
target_arch="${RAYNET_ARCH:-}"
raynet_python=""

python_arch() {
    local python=$1
    "$python" -c 'import platform; print(platform.machine())' 2>/dev/null || true
}

if [ -x "$RAYNET_HOME/.venv/bin/python" ]; then
    raynet_python="$RAYNET_HOME/.venv/bin/python"
elif [ -n "${VIRTUAL_ENV:-}" ] && [ -x "$VIRTUAL_ENV/bin/python" ]; then
    raynet_python="$VIRTUAL_ENV/bin/python"
fi

if [ -z "$target_arch" ] && [ -n "$raynet_python" ]; then
    target_arch=$(python_arch "$raynet_python")
fi
target_arch=${target_arch:-$host_machine}

# Usage info
show_help(){

cat <<EOF
Usage: ${0##*/} [-h] [-m BUILDMODE] [-o OMNETPP_ROOT] [-i INET_ROOT]...
	Build (compile and link) Raynet BUILDMODE mode.

       -h            display this help and exit
       -b            make with 'bear -- make' to generate compile commands (requires bear)
       -c            clean simulation libarries before building
       -r            rebuild (deletes the current build directory before proceeding)
       -m BUILDMODE  chose between release and debug modes. Defaults to release.
       -o ROOT       use a specific OMNeT++ root. Defaults to OMNETPP_ROOT or newest ~/omnetpp-*.
       -i ROOT       use a specific INET root. Defaults to INET_ROOT or OMNETPP_ROOT/samples/inet*.

Environment:
       RAYNET_BUILD_CONFIG  optional config file to source before building.
                            Defaults to .raynet-build.env in the RayNet root.
EOF
}
   
   # Initialize our own variables:
   mode="release"
   make_prefix=""
   clean="false"
   rebuild="false"
   omnetpp_root_option=""
   inet_root_option=""

   OPTIND=1
   # Resetting OPTIND is necessary if getopts was used previously in the script.
   # It is a good idea to make OPTIND local if you process options in a function.
   
   while getopts hbcrm:o:i: opt; do
       case $opt in
           h)
               show_help
               exit 0
               ;;
           b)  
		   	   make_prefix="bear -- "
			   ;;
           c)
		       clean='true'
			   ;;
           r)
               rebuild='true'
               ;;
           m)  mode=$OPTARG
               ;;
           o)  omnetpp_root_option=$OPTARG
               ;;
           i)  inet_root_option=$OPTARG
               ;;
			   
           *)
               show_help >&2
               exit 1
               ;;
       esac
   done
   shift "$((OPTIND-1))"   # Discard the options and sentinel --

# Check for invalid build mode
if [ "$mode" != "release" ] && [ "$mode" != "debug" ] 
then
	echo "-m option value not recognised. Select between release and debug"
	echo "Build failed."	
	exit 1 
	fi

# Command/flags have been validated. Begin building here.
# ----------------------------------------------------------------------------------------------------------------------------------

die() {
    echo "Build failed: $*" >&2
    exit 1
}

resolve_dir() {
    local dir=$1
    [ -d "$dir" ] || return 1
    (cd "$dir" >/dev/null 2>&1 && pwd -P)
}

is_omnetpp_root() {
    local dir=$1
    [ -f "$dir/setenv" ] && [ -d "$dir/bin" ] && [ -f "$dir/Makefile.inc" ]
}

binary_arch() {
    local path=$1
    [ -e "$path" ] || return 0
    file "$path" 2>/dev/null | grep -Eo 'arm64|x86_64' | head -n 1
}

omnetpp_binary_arch() {
    local dir=$1
    local probe

    for probe in "$dir/lib/liboppsim.dylib" "$dir/bin/opp_run"; do
        [ -e "$probe" ] || continue
        binary_arch "$probe"
        return
    done
}

is_compatible_omnetpp_root() {
    local dir=$1
    local arch

    [ "$host_system" = "Darwin" ] || return 0
    arch=$(omnetpp_binary_arch "$dir")
    [ -z "$arch" ] && return 0
    [ "$arch" = "$target_arch" ]
}

select_omnetpp_root() {
    local requested=$1
    local candidate
    local candidates
    local i

    if [ -n "$requested" ]; then
        is_omnetpp_root "$requested" || return 1
        is_compatible_omnetpp_root "$requested" || {
            echo "OMNeT++ at $requested is $(omnetpp_binary_arch "$requested"), but this build needs $target_arch." >&2
            return 1
        }
        resolve_dir "$requested"
        return
    fi

    if [ -n "$OMNETPP_ROOT" ]; then
        is_omnetpp_root "$OMNETPP_ROOT" || return 1
        is_compatible_omnetpp_root "$OMNETPP_ROOT" || {
            echo "OMNeT++ at $OMNETPP_ROOT is $(omnetpp_binary_arch "$OMNETPP_ROOT"), but this build needs $target_arch." >&2
            return 1
        }
        resolve_dir "$OMNETPP_ROOT"
        return
    fi

    candidates=("$HOME"/omnetpp-*)
    for ((i=${#candidates[@]}-1; i>=0; i--)); do
        candidate=${candidates[$i]}
        [ -d "$candidate" ] || continue
        [ -L "$candidate" ] && continue
        if is_omnetpp_root "$candidate"; then
            is_compatible_omnetpp_root "$candidate" || {
                echo "Found OMNeT++ at $candidate, but it is $(omnetpp_binary_arch "$candidate") and this build needs $target_arch." >&2
                echo "Use matching architectures for Python and OMNeT++, or pass -o to a compatible install explicitly." >&2
                return 1
            }
            resolve_dir "$candidate"
            return
        fi
    done

    candidate="$HOME/omnetpp"
    if is_omnetpp_root "$candidate"; then
        is_compatible_omnetpp_root "$candidate" || {
            echo "Skipping OMNeT++: $candidate ($(omnetpp_binary_arch "$candidate"), need $target_arch)"
            return 1
        }
        resolve_dir "$candidate"
        return
    fi

    return 1
}

find_sample_root() {
    local env_name=$1
    local label=$2
    shift 2
    local requested=${!env_name}
    local candidate
    local candidates=("$@")
    local i

    if [ -n "$requested" ]; then
        [ -d "$requested/src" ] || return 1
        resolve_dir "$requested"
        return
    fi

    for ((i=${#candidates[@]}-1; i>=0; i--)); do
        candidate=${candidates[$i]}
        [ -d "$candidate/src" ] || continue
        resolve_dir "$candidate"
        return
    done

    echo "Could not find $label. Set $env_name or pass the relevant build option." >&2
    return 1
}

OMNETPP_ROOT=$(select_omnetpp_root "$omnetpp_root_option") || die "Could not find a usable OMNeT++ root. Pass -o /path/to/omnetpp or set OMNETPP_ROOT."
export OMNETPP_ROOT

source "$OMNETPP_ROOT/setenv"
export OMNETPP_ROOT

OMNETPP_SAMPLES_ROOT=${OMNETPP_SAMPLES_ROOT:-"$OMNETPP_ROOT/samples"}
OMNETPP_SAMPLES_ROOT=$(resolve_dir "$OMNETPP_SAMPLES_ROOT") || die "Could not find OMNeT++ samples directory at $OMNETPP_SAMPLES_ROOT"
export OMNETPP_SAMPLES_ROOT

[ -n "$inet_root_option" ] && INET_ROOT=$inet_root_option
INET_ROOT=$(find_sample_root INET_ROOT INET "$OMNETPP_SAMPLES_ROOT"/inet*) || die "Could not find INET under $OMNETPP_SAMPLES_ROOT. Pass -i /path/to/inet or set INET_ROOT."
TCPPACED_ROOT=$(find_sample_root TCPPACED_ROOT tcpPaced "$OMNETPP_SAMPLES_ROOT"/tcpPaced) || die "Could not find tcpPaced under $OMNETPP_SAMPLES_ROOT. Set TCPPACED_ROOT."
CUBIC_ROOT=$(find_sample_root CUBIC_ROOT cubic "$OMNETPP_SAMPLES_ROOT"/cubic) || die "Could not find cubic under $OMNETPP_SAMPLES_ROOT. Set CUBIC_ROOT."
TCPGOODPUT_ROOT=$(find_sample_root TCPGOODPUT_ROOT tcpGoodputApplications "$OMNETPP_SAMPLES_ROOT"/tcpGoodputApplications) || die "Could not find tcpGoodputApplications under $OMNETPP_SAMPLES_ROOT. Set TCPGOODPUT_ROOT."
LEOSATELLITES_ROOT=$(find_sample_root LEOSATELLITES_ROOT leosatellites "$OMNETPP_SAMPLES_ROOT"/leosatellites) || die "Could not find leosatellites under $OMNETPP_SAMPLES_ROOT. Set LEOSATELLITES_ROOT."
ORBTCPEXPERIMENTS_ROOT=$(find_sample_root ORBTCPEXPERIMENTS_ROOT orbtcpExperiments "$OMNETPP_SAMPLES_ROOT"/orbtcpExperiments) || die "Could not find orbtcpExperiments under $OMNETPP_SAMPLES_ROOT. Set ORBTCPEXPERIMENTS_ROOT."

export INET_ROOT TCPPACED_ROOT CUBIC_ROOT TCPGOODPUT_ROOT LEOSATELLITES_ROOT ORBTCPEXPERIMENTS_ROOT

echo "Using OMNeT++: $OMNETPP_ROOT"
echo "Using INET:    $INET_ROOT"
echo "Using samples: $OMNETPP_SAMPLES_ROOT"
echo "Target arch:   $target_arch"

macosx_sdk_arg=()
if command -v xcrun >/dev/null 2>&1; then
    macosx_sdk_path=$(xcrun --show-sdk-path 2>/dev/null)
    if [ -n "$macosx_sdk_path" ]; then
        macosx_sdk_arg=(-DCMAKE_OSX_SYSROOT="$macosx_sdk_path")
    fi
fi

cmake_arch_arg=()
if [ "$host_system" = "Darwin" ]; then
    cmake_arch_arg=(-DCMAKE_OSX_ARCHITECTURES="$target_arch")
fi

python_cmake_args=(-DPython3_FIND_VIRTUALENV=FIRST)
if [ -n "$raynet_python" ]; then
    python_cmake_args+=(-DPython3_EXECUTABLE="$raynet_python")
    python_include=$("$raynet_python" - <<'PY'
import os
import sysconfig

seen = set()
for value in (
    sysconfig.get_path("include"),
    sysconfig.get_path("platinclude"),
    sysconfig.get_config_var("INCLUDEPY"),
):
    if not value or value in seen:
        continue
    seen.add(value)
    if os.path.isfile(os.path.join(value, "Python.h")):
        print(value)
        break
PY
)
    if [ -n "$python_include" ]; then
        python_cmake_args+=(-DPython3_INCLUDE_DIR="$python_include")
    else
        echo "Warning: could not locate Python.h for $raynet_python. Install the matching python3-dev package if CMake cannot find it." >&2
    fi
fi

cmake_python_cache_unset_args=(-U "Python3_*" -U "_Python3_*" -U "Python_*" -U "_Python_*" -U "PYTHON_*" -U "PYBIND11_PYTHON_*")

# List of simlibs. Any simlib that needs to be compiled FIRST (eg. is a dependency) should be added here, rather than the loop.
simlibs=("$RAYNET_HOME/simlibs/RLComponents"
"$RAYNET_HOME/simlibs/tcpPacedNoCC"
)

# list directories in the form "/tmp/dirname/"
for dir in $RAYNET_HOME/simlibs/*/     
do
    dir=${dir%*/}      # remove the trailing "/"
    exists=0
    for simlib in "${simlibs[@]}" 
    do
        if [ "${simlib}" = "${dir}" ] 
        then
            exists=1
        fi
    done
    if [ ${exists} = 0 ] # Only append the simlib to the list if it was not previously included
    then
        simlibs=("${simlibs[@]}" ${dir})    # Append the simulation library to the simlibs list
    fi
done

# Debug print:
echo "Simulation libraries detected:"
echo ${simlibs[@]}
for simlib in "${simlibs[@]}"; do
    echo ${simlib}
done

# -c flag: Clean simulation libraries
if [ "$clean" = "true" ]; then
    echo "Cleaning simulation libararies..."
    echo ""

    for proj in "${simlibs[@]}"; do
        echo "Cleaning $proj..."
        echo "---------------------------------"
        cd "$proj" || exit 1
        ${make_prefix}make cleanall
        echo "---------------------------------"
        echo ""
    done
    echo "Cleaning complete!"
    echo ""
fi

# Build INET (release or debug)
echo "Building INET $mode..."
echo "---------------------------------"
cd "$INET_ROOT" && make -j32 MODE=$mode
echo "---------------------------------"
echo ""

# Build simulation libraries (release or debug)
for proj in "${simlibs[@]}"; do
    echo "Building $mode libraries in $proj..."
    echo "---------------------------------"
    cd "$proj" || exit 1
    # ${make_prefix}make makefiles$mode       # Generate makefile in the simlib src directory using opp_makemake
    ${make_prefix}make makefiles MODE=$mode   # Just in case the simlib only has a make makefiles option, and no make makefilesrelease or make makefilesdebug (cubic does this)
    ${make_prefix}make -j32 MODE=$mode      # Build simlib with the generated makefile
    echo "---------------------------------"
    echo ""
done

# -r flag: Remove any existing build directory before building
if [ "$rebuild" = "true" ]; then  
    cd "$RAYNET_HOME"
    rm -rf build 
fi

# Build raynet (release or debug)
cd "$RAYNET_HOME"
mkdir -p build 
cd build
echo "Building RayNet..."
echo "---------------------------------"
cmake -DCMAKE_BUILD_TYPE=$mode \
    "${cmake_python_cache_unset_args[@]}" \
    "${macosx_sdk_arg[@]}" \
    "${cmake_arch_arg[@]}" \
    "${python_cmake_args[@]}" \
    -DOMNETPP_ROOT="$OMNETPP_ROOT" \
    -DOMNETPP_SAMPLES_ROOT="$OMNETPP_SAMPLES_ROOT" \
    -DINET_ROOT="$INET_ROOT" \
    -DTCPGOODPUT_ROOT="$TCPGOODPUT_ROOT" \
    -DLEOSATELLITES_ROOT="$LEOSATELLITES_ROOT" \
    -DORBTCPEXPERIMENTS_ROOT="$ORBTCPEXPERIMENTS_ROOT" \
    ../
make -j32
echo "---------------------------------"
