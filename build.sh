#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="$ROOT/.venv"

PYTHON_BIN="${PYTHON:-}"
if [ -z "$PYTHON_BIN" ]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="python3"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="python"
    else
        echo "ERROR: no python3/python found on PATH" >&2
        exit 1
    fi
fi

if [ ! -x "$VENV/bin/python" ]; then
    echo "=== Creating virtualenv at $VENV (using $PYTHON_BIN) ==="
    "$PYTHON_BIN" -m venv "$VENV"
fi

PY="$VENV/bin/python"
PIP="$VENV/bin/pip"

echo ""
echo "=== Upgrading pip ==="
"$PY" -m pip install --upgrade pip

echo ""
echo "=== Installing Python dependencies ==="
"$PIP" install pybind11 scikit-build-core
"$PIP" install -e "$ROOT/.[web,dev]" --no-build-isolation

echo ""
echo "=== Configuring CMake ==="
PYBIND11_DIR=$("$PY" -c "import pybind11; print(pybind11.get_cmake_dir())")
cmake -S "$ROOT" -B "$ROOT/build_cpp" \
    -DCMAKE_BUILD_TYPE=Release \
    "-DPython_EXECUTABLE=$PY" \
    "-Dpybind11_DIR=$PYBIND11_DIR" \
    -DEXCO_BUILD_TESTS=OFF \
    -Wno-dev

echo ""
echo "=== Building C++ extension ==="
cmake --build "$ROOT/build_cpp" --config Release

echo ""
echo "=== Copying extension to package ==="
PYD=$(find "$ROOT/build_cpp" -name "_exco_cpp*.so" | head -1)
if [ -z "$PYD" ]; then
    echo "ERROR: Build failed: _exco_cpp*.so not found in build_cpp" >&2
    exit 1
fi
FILENAME=$(basename "$PYD")
cp "$PYD" "$ROOT/python/exco/$FILENAME"
echo "Installed: $FILENAME"

echo ""
echo "=== Verifying import ==="
"$PY" -c "import exco._exco_cpp as cpp; print('OK:', [x for x in dir(cpp) if not x.startswith('_')])"

echo ""
echo "=== Done ==="
echo "Activate the venv with: source $VENV/bin/activate"
