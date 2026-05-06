# Build Instructions

## Prerequisites

- Python 3.11+
- CMake 3.20+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- pip

## Quick Start (Linux / macOS)

```bash
git clone https://github.com/vGubriienko/exercises-counter.git
cd exercises-counter
python -m venv .venv
source .venv/bin/activate
pip install -e ".[web,dev]"
python -c "import exco._exco_cpp; print('OK')"
```

## Raspberry Pi 5

Same as Linux. MediaPipe supports aarch64:

```bash
pip install -e ".[web]"
```

For camera access:
```bash
sudo usermod -aG video $USER
```

## C++ Tests Only

```bash
mkdir build && cd build
cmake .. -DEXCO_BUILD_TESTS=ON
cmake --build .
./cpp/exco_tests
```

## Windows

Requires Visual Studio 2019+ with C++ workload:

```powershell
pip install -e ".[web,dev]"
```

## macOS (Apple Silicon)

Works natively:

```bash
pip install -e ".[web,dev]"
```

## iOS Integration

The C++ core (`exco_core`) compiles on arm64 and has zero dependencies beyond STL.
`PoseDetector` is a protocol-based interface — iOS developers can implement it using
Apple Vision (`VNDetectHumanBodyPoseRequest`) instead of MediaPipe.

Integration path:
1. Add `cpp/` sources to an Xcode project (C++17, no extra libs needed)
2. Implement `PoseDetector` protocol using Vision framework
3. Feed landmarks to `AnalyzerCore::push_frame()` via the C++ API

No standalone iOS build target is included — this is a library meant to be
embedded into a native iOS app.
