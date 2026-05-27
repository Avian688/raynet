#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
venv_dir="${VENV_DIR:-$repo_dir/.venv}"
python_bin="${PYTHON:-}"
requirements_file="$repo_dir/requirements.txt"
install_extra=false
python_set_explicitly=false
recreate=false
target_arch="${RAYNET_ARCH:-}"
host_system="$(uname -s)"
host_machine="$(uname -m)"
if [ "$host_system" = "Darwin" ]; then
    if native_machine=$(arch -arm64 /usr/bin/uname -m 2>/dev/null) && [ "$native_machine" = "arm64" ]; then
        host_machine=arm64
    fi
fi

show_help() {
    cat <<EOF
Usage: ${0##*/} [--extra] [--recreate] [--arch ARCH] [--python PYTHON] [--venv DIR] [--requirements FILE]

Create RayNet's Python virtual environment and install dependencies.

Options:
  --extra              also install requirements-extra.txt
  --recreate           remove the target venv first if it already exists
  --arch ARCH          target architecture: arm64 or x86_64. Defaults to RAYNET_ARCH, existing omnetbind, or host.
  --python PYTHON      Python executable to use. Defaults to PYTHON or a compatible Homebrew Python.
  --venv DIR           virtual environment directory. Defaults to VENV_DIR or .venv.
  --requirements FILE  requirements file for the base install. Defaults to requirements.txt.
  -h, --help           show this help
EOF
}

require_value() {
    if [ "$#" -lt 2 ] || [[ "$2" == -* ]]; then
        echo "Missing value for $1" >&2
        exit 1
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --extra)
            install_extra=true
            shift
            ;;
        --recreate)
            recreate=true
            shift
            ;;
        --arch)
            require_value "$@"
            target_arch=$2
            shift 2
            ;;
        --python)
            require_value "$@"
            python_bin=$2
            python_set_explicitly=true
            shift 2
            ;;
        --venv)
            require_value "$@"
            venv_dir=$2
            shift 2
            ;;
        --requirements)
            require_value "$@"
            requirements_file=$2
            shift 2
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            show_help >&2
            exit 1
            ;;
    esac
done

normalize_path() {
    local path=$1
    local parent
    local name
    parent=$(dirname "$path")
    name=$(basename "$path")
    (cd "$parent" >/dev/null 2>&1 && printf '%s/%s\n' "$(pwd -P)" "$name")
}

venv_dir=$(normalize_path "$venv_dir")

binary_arch() {
    local path=$1
    [ -e "$path" ] || return 0
    file "$path" 2>/dev/null | grep -Eo 'arm64|x86_64' | head -n 1
}

if [ -z "$target_arch" ] && [ -d "$repo_dir/build" ]; then
    existing_bind=$(find "$repo_dir/build" -maxdepth 1 -type f -name 'omnetbind*.so' | head -n 1)
    if [ -n "$existing_bind" ]; then
        target_arch=$(binary_arch "$existing_bind")
    fi
fi

target_arch=${target_arch:-$host_machine}
case "$target_arch" in
    arm64|x86_64)
        ;;
    *)
        echo "Unsupported architecture: $target_arch" >&2
        echo "Use --arch arm64 or --arch x86_64." >&2
        exit 1
        ;;
esac

is_inside_target_venv() {
    local path=$1
    [[ "$path" == "$venv_dir/"* ]]
}

is_supported_python() {
    local python=$1
    "$python" - "$host_system" "$target_arch" <<'PY'
import platform
import sys

host_system = sys.argv[1]
target_arch = sys.argv[2]
version = sys.version_info[:2]
python_machine = platform.machine()

if host_system == "Darwin" and python_machine != target_arch:
    print(
        f"Python architecture {python_machine} does not match RayNet target architecture ({target_arch}).",
        file=sys.stderr,
    )
    raise SystemExit(1)

if host_system == "Darwin" and target_arch == "x86_64":
    if version != (3, 12):
        print(
            f"Python {version[0]}.{version[1]} is not supported for RayNet's macOS x86_64/Rosetta environment.",
            file=sys.stderr,
        )
        print(
            "Use Python 3.12 from /usr/local, e.g. ./create-venv.sh --arch x86_64 --python /usr/local/bin/python3.12",
            file=sys.stderr,
        )
        raise SystemExit(1)
    raise SystemExit(0)

if not ((3, 10) <= version <= (3, 13)):
    print(
        f"Python {version[0]}.{version[1]} is not supported by RayNet's pinned dependencies.",
        file=sys.stderr,
    )
    print(
        "Use Python 3.12 if possible, e.g. ./create-venv.sh --python /opt/homebrew/bin/python3.12",
        file=sys.stderr,
    )
    raise SystemExit(1)
PY
}

find_python() {
    local candidate
    local resolved
    local candidates

    if [ "$host_system" = "Darwin" ] && [ "$target_arch" = "x86_64" ]; then
        candidates=(
            /usr/local/bin/python3.12
            /usr/local/opt/python@3.12/bin/python3.12
            python3.12
        )
    else
        candidates=(
            /opt/homebrew/bin/python3.12
            python3.12
            /usr/local/bin/python3.12
            /opt/homebrew/bin/python3.11
            python3.11
            /usr/local/bin/python3.11
            /opt/homebrew/bin/python3.10
            python3.10
            /usr/local/bin/python3.10
            /opt/homebrew/bin/python3.13
            python3.13
            /usr/local/bin/python3.13
        )
    fi

    for candidate in "${candidates[@]}"; do
        if [[ "$candidate" == */* ]]; then
            [ -x "$candidate" ] || continue
            resolved=$candidate
        else
            resolved=$(command -v "$candidate" 2>/dev/null) || continue
        fi
        is_inside_target_venv "$resolved" && continue

        if is_supported_python "$resolved" >/dev/null 2>&1
        then
            echo "$resolved"
            return 0
        fi
    done

    return 1
}

if [ ! -f "$requirements_file" ]; then
    echo "Requirements file not found: $requirements_file" >&2
    exit 1
fi

if [ -z "$python_bin" ]; then
    python_bin=$(find_python) || {
        echo "Could not find a supported $target_arch Python for RayNet dependencies." >&2
        echo "For Rosetta OMNeT++ use: ${0##*/} --arch x86_64 --python /usr/local/bin/python3.12" >&2
        echo "For native Apple Silicon use: ${0##*/} --arch arm64 --python /opt/homebrew/bin/python3.12" >&2
        exit 1
    }
elif [[ "$python_bin" != */* ]]; then
    python_bin=$(command -v "$python_bin") || {
        echo "Python executable not found: $python_bin" >&2
        exit 1
    }
fi

if is_inside_target_venv "$python_bin"; then
    echo "Refusing to create $venv_dir using Python from inside that same venv:" >&2
    echo "  $python_bin" >&2
    echo "Run with an external interpreter, e.g. ./create-venv.sh --recreate --arch $target_arch --python /path/to/python3.12" >&2
    exit 1
fi

if ! is_supported_python "$python_bin"
then
    if [ "$python_set_explicitly" = true ]; then
        exit 1
    fi
    exit 1
fi

echo "Target architecture: $target_arch"
echo "Using Python: $("$python_bin" -c 'import sys; print(sys.executable + " (" + sys.version.split()[0] + ")")')"

if [ -d "$venv_dir" ]; then
    if [ "$recreate" = true ]; then
        rm -rf "$venv_dir"
    elif [ -x "$venv_dir/bin/python" ]; then
        if ! is_supported_python "$venv_dir/bin/python" >/dev/null 2>&1
        then
            echo "Existing venv at $venv_dir uses an unsupported Python or architecture." >&2
            echo "Recreate it with: ./create-venv.sh --recreate" >&2
            echo "If it is currently active, run 'deactivate' first." >&2
            exit 1
        fi

        selected_version=$("$python_bin" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
        existing_version=$("$venv_dir/bin/python" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
        if [ "$selected_version" != "$existing_version" ]; then
            echo "Existing venv at $venv_dir uses Python $existing_version, but selected Python is $selected_version." >&2
            echo "Recreate it with: ./create-venv.sh --recreate" >&2
            echo "If it is currently active, run 'deactivate' first." >&2
            exit 1
        fi
    else
        echo "Existing directory is not a Python venv: $venv_dir" >&2
        echo "Remove it or run with --venv DIR to choose another location." >&2
        exit 1
    fi
fi

"$python_bin" -m venv "$venv_dir"
source "$venv_dir/bin/activate"

python -m pip install --upgrade pip
python -m pip install -r "$requirements_file"

if [ "$install_extra" = true ]; then
    python -m pip install -r "$repo_dir/requirements-extra.txt"
fi

echo ""
echo "----------------------------------------"
echo "Virtual environment installed. Activate it before running RayNet:"
echo "    source \"$venv_dir/bin/activate\""
echo "----------------------------------------"
echo ""
