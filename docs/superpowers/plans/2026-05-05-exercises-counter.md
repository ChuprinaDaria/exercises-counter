# Exercises Counter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a prototype that automatically detects and counts repeating exercise patterns from video, using two decoupled processes (Writer + Analyzer) communicating via SQLite.

**Architecture:** Writer process captures video frames, runs MediaPipe pose detection, writes landmarks to SQLite. Analyzer process polls DB, runs C++ signal analysis to detect periodic patterns, counts repetitions, persists patterns immediately. Web UI reads events via WebSocket.

**Tech Stack:** C++17 (STL only), pybind11, CMake 3.20+, Python 3.11+, MediaPipe, OpenCV, SQLite, FastAPI, WebSocket

---

## File Map

| File | Responsibility |
|------|---------------|
| `CMakeLists.txt` | Top-level CMake — delegates to cpp/ |
| `pyproject.toml` | Python package config with scikit-build-core |
| `cpp/CMakeLists.txt` | C++ library + pybind11 module + doctest runner |
| `cpp/include/exco/geometry.hpp` | angle_between, distance, normalize — header-only |
| `cpp/include/exco/signal.hpp` | SignalProcessor: smooth, autocorrelate, find_period |
| `cpp/src/signal.cpp` | SignalProcessor implementation |
| `cpp/include/exco/pattern.hpp` | Pattern struct, PatternMatcher: extract, DTW, serialize |
| `cpp/src/pattern.cpp` | PatternMatcher implementation |
| `cpp/include/exco/counter.hpp` | RepCounter: Schmitt trigger state machine |
| `cpp/src/counter.cpp` | RepCounter implementation |
| `cpp/include/exco/analyzer_core.hpp` | AnalyzerCore: orchestrates signal→pattern→counter pipeline |
| `cpp/src/analyzer_core.cpp` | AnalyzerCore implementation |
| `cpp/bindings/python_bindings.cpp` | pybind11 module `_exco_cpp` |
| `cpp/tests/test_geometry.cpp` | doctest for geometry |
| `cpp/tests/test_signal.cpp` | doctest for signal |
| `cpp/tests/test_pattern.cpp` | doctest for pattern |
| `cpp/tests/test_counter.cpp` | doctest for counter |
| `cpp/tests/test_main.cpp` | doctest main entry |
| `python/exco/__init__.py` | Re-export from C++ extension + Python classes |
| `python/exco/events.py` | ExerciseEvent dataclass |
| `python/exco/db.py` | SQLite schema, read/write helpers |
| `python/exco/pose/base.py` | PoseDetector Protocol, Landmark dataclass |
| `python/exco/pose/mediapipe_backend.py` | MediaPipe implementation |
| `python/exco/writer.py` | LandmarkWriter process |
| `python/exco/analyzer.py` | PatternAnalyzer process |
| `demo/cli.py` | CLI entry point — Writer + Analyzer + stdout |
| `demo/web/server.py` | FastAPI + WebSocket server |
| `demo/web/static/index.html` | Browser dashboard |
| `tests/test_db.py` | SQLite helpers integration test |
| `tests/test_pipeline.py` | End-to-end: synthetic landmarks → events |
| `docs/BUILD.md` | Build instructions |
| `docs/ARCHITECTURE.md` | Architecture overview |
| `docs/TUNING.md` | Threshold tuning guide |
| `.github/workflows/build.yml` | CI: multi-platform build + test |

---

## Task 1: Project Scaffolding

**Files:**
- Create: `CMakeLists.txt`
- Create: `pyproject.toml`
- Create: `cpp/CMakeLists.txt`
- Create: `cpp/tests/test_main.cpp`
- Create: `python/exco/__init__.py`

- [ ] **Step 1: Create top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(exercises_counter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(cpp)
```

- [ ] **Step 2: Create pyproject.toml**

```toml
[build-system]
requires = ["scikit-build-core>=0.5", "pybind11>=2.11"]
build-backend = "scikit_build_core.build"

[project]
name = "exco"
version = "0.1.0"
description = "Exercise counter — automatic repetition detection from video"
requires-python = ">=3.11"
dependencies = [
    "mediapipe>=0.10",
    "opencv-python>=4.8",
    "numpy>=1.24",
]

[project.optional-dependencies]
web = ["fastapi>=0.104", "uvicorn>=0.24", "websockets>=12.0"]
dev = ["pytest>=7.4", "mypy>=1.7"]

[tool.scikit-build]
cmake.source-dir = "."
wheel.packages = ["python/exco"]
wheel.install-dir = "exco"
build-dir = "build/{wheel_tag}"

[tool.pytest.ini_options]
testpaths = ["tests"]
pythonpath = ["python"]

[tool.mypy]
strict = true
python_version = "3.11"
mypy_path = "python"
```

- [ ] **Step 3: Create cpp/CMakeLists.txt with placeholder library**

```cmake
set(EXCO_SOURCES
    src/signal.cpp
    src/pattern.cpp
    src/counter.cpp
    src/analyzer_core.cpp
)

set(EXCO_HEADERS
    include/exco/geometry.hpp
    include/exco/signal.hpp
    include/exco/pattern.hpp
    include/exco/counter.hpp
    include/exco/analyzer_core.hpp
)

add_library(exco_core STATIC ${EXCO_SOURCES})
target_include_directories(exco_core PUBLIC include)
target_compile_features(exco_core PUBLIC cxx_std_17)

# pybind11 module
find_package(pybind11 CONFIG QUIET)
if(pybind11_FOUND)
    pybind11_add_module(_exco_cpp bindings/python_bindings.cpp)
    target_link_libraries(_exco_cpp PRIVATE exco_core)
    install(TARGETS _exco_cpp DESTINATION exco)
endif()

# doctest
option(EXCO_BUILD_TESTS "Build tests" ON)
if(EXCO_BUILD_TESTS)
    add_executable(exco_tests
        tests/test_main.cpp
    )
    target_link_libraries(exco_tests PRIVATE exco_core)
    target_include_directories(exco_tests PRIVATE tests)
    enable_testing()
    add_test(NAME exco_tests COMMAND exco_tests)
endif()
```

- [ ] **Step 4: Create doctest main entry**

Download doctest header and create test main:

```bash
mkdir -p cpp/tests
curl -sL https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h -o cpp/tests/doctest.h
```

File `cpp/tests/test_main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

- [ ] **Step 5: Create empty source files so CMake can configure**

`cpp/src/signal.cpp`:
```cpp
#include "exco/signal.hpp"
```

`cpp/src/pattern.cpp`:
```cpp
#include "exco/pattern.hpp"
```

`cpp/src/counter.cpp`:
```cpp
#include "exco/counter.hpp"
```

`cpp/src/analyzer_core.cpp`:
```cpp
#include "exco/analyzer_core.hpp"
```

Create minimal headers so the project compiles:

`cpp/include/exco/geometry.hpp`:
```cpp
#pragma once
namespace exco {}
```

`cpp/include/exco/signal.hpp`:
```cpp
#pragma once
namespace exco {}
```

`cpp/include/exco/pattern.hpp`:
```cpp
#pragma once
namespace exco {}
```

`cpp/include/exco/counter.hpp`:
```cpp
#pragma once
namespace exco {}
```

`cpp/include/exco/analyzer_core.hpp`:
```cpp
#pragma once
namespace exco {}
```

`cpp/bindings/python_bindings.cpp`:
```cpp
#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(_exco_cpp, m) {
    m.doc() = "Exercise counter C++ core";
}
```

`python/exco/__init__.py`:
```python
"""Exercise counter — automatic repetition detection from video."""
```

- [ ] **Step 6: Verify CMake configures and builds**

```bash
cd /home/dchuprina/exercises-counter
mkdir -p build && cd build
cmake .. -DEXCO_BUILD_TESTS=ON
cmake --build .
./cpp/exco_tests
```

Expected: 0 tests, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt pyproject.toml cpp/ python/exco/__init__.py
git commit -m "feat: project scaffolding — CMake, pyproject.toml, empty C++ core"
```

---

## Task 2: C++ Geometry Utilities (header-only)

**Files:**
- Create: `cpp/include/exco/geometry.hpp`
- Create: `cpp/tests/test_geometry.cpp`

- [ ] **Step 1: Write failing tests for geometry**

`cpp/tests/test_geometry.cpp`:
```cpp
#include "doctest.h"
#include "exco/geometry.hpp"
#include <cmath>

TEST_CASE("distance_2d") {
    CHECK(exco::distance_2d(0.0f, 0.0f, 3.0f, 4.0f) == doctest::Approx(5.0f));
    CHECK(exco::distance_2d(1.0f, 1.0f, 1.0f, 1.0f) == doctest::Approx(0.0f));
}

TEST_CASE("angle_between_three_points returns degrees") {
    // Straight line → 180 degrees
    float angle = exco::angle_between(0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f);
    CHECK(angle == doctest::Approx(180.0f).epsilon(0.01));

    // Right angle
    float right = exco::angle_between(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    CHECK(right == doctest::Approx(90.0f).epsilon(0.01));
}

TEST_CASE("normalize_to_range") {
    CHECK(exco::normalize(5.0f, 0.0f, 10.0f) == doctest::Approx(0.5f));
    CHECK(exco::normalize(0.0f, 0.0f, 10.0f) == doctest::Approx(0.0f));
    CHECK(exco::normalize(10.0f, 0.0f, 10.0f) == doctest::Approx(1.0f));
}
```

- [ ] **Step 2: Add test file to CMakeLists.txt**

In `cpp/CMakeLists.txt`, update the test target:
```cmake
if(EXCO_BUILD_TESTS)
    add_executable(exco_tests
        tests/test_main.cpp
        tests/test_geometry.cpp
    )
    target_link_libraries(exco_tests PRIVATE exco_core)
    target_include_directories(exco_tests PRIVATE tests)
    enable_testing()
    add_test(NAME exco_tests COMMAND exco_tests)
endif()
```

- [ ] **Step 3: Run tests — verify they fail**

```bash
cd build && cmake .. && cmake --build .
./cpp/exco_tests
```

Expected: compilation errors (functions not defined).

- [ ] **Step 4: Implement geometry.hpp**

`cpp/include/exco/geometry.hpp`:
```cpp
#pragma once
#include <cmath>

namespace exco {

inline float distance_2d(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

inline float angle_between(float ax, float ay,
                           float bx, float by,
                           float cx, float cy) {
    float bax = ax - bx;
    float bay = ay - by;
    float bcx = cx - bx;
    float bcy = cy - by;
    float dot = bax * bcx + bay * bcy;
    float cross = bax * bcy - bay * bcx;
    float rad = std::atan2(std::abs(cross), dot);
    return rad * 180.0f / static_cast<float>(M_PI);
}

inline float normalize(float value, float min_val, float max_val) {
    if (max_val <= min_val) return 0.0f;
    return (value - min_val) / (max_val - min_val);
}

} // namespace exco
```

- [ ] **Step 5: Run tests — verify they pass**

```bash
cd build && cmake --build . && ./cpp/exco_tests
```

Expected: 3 test cases, all pass.

- [ ] **Step 6: Commit**

```bash
git add cpp/include/exco/geometry.hpp cpp/tests/test_geometry.cpp cpp/CMakeLists.txt
git commit -m "feat(core): geometry utilities — distance, angle, normalize"
```

---

## Task 3: C++ Signal Processing

**Files:**
- Modify: `cpp/include/exco/signal.hpp`
- Modify: `cpp/src/signal.cpp`
- Create: `cpp/tests/test_signal.cpp`

- [ ] **Step 1: Write failing tests**

`cpp/tests/test_signal.cpp`:
```cpp
#include "doctest.h"
#include "exco/signal.hpp"
#include <cmath>
#include <vector>

TEST_CASE("smooth — moving average removes noise") {
    // Clean sine + noise → smoothed should be closer to sine
    std::vector<float> noisy = {0.0f, 1.2f, 0.1f, 0.9f, 0.0f, 1.1f, -0.1f, 1.0f};
    auto smoothed = exco::smooth(noisy, 3);
    CHECK(smoothed.size() == noisy.size());
    // After smoothing, variance should be lower
    float var_orig = 0.0f, var_smooth = 0.0f;
    float mean_o = 0.0f, mean_s = 0.0f;
    for (size_t i = 0; i < noisy.size(); ++i) {
        mean_o += noisy[i];
        mean_s += smoothed[i];
    }
    mean_o /= static_cast<float>(noisy.size());
    mean_s /= static_cast<float>(smoothed.size());
    for (size_t i = 0; i < noisy.size(); ++i) {
        var_orig += (noisy[i] - mean_o) * (noisy[i] - mean_o);
        var_smooth += (smoothed[i] - mean_s) * (smoothed[i] - mean_s);
    }
    CHECK(var_smooth < var_orig);
}

TEST_CASE("autocorrelate — detects periodicity") {
    // Generate sine wave with period 10 samples
    std::vector<float> signal;
    for (int i = 0; i < 100; ++i) {
        signal.push_back(std::sin(2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / 10.0f));
    }
    auto acorr = exco::autocorrelate(signal);
    CHECK(acorr.size() == signal.size());
    // Peak at lag 0 should be highest
    CHECK(acorr[0] >= acorr[1]);
    // Peak at lag 10 (the period) should be a local max
    CHECK(acorr[10] > acorr[7]);
    CHECK(acorr[10] > acorr[13]);
}

TEST_CASE("find_period — returns correct period for sine") {
    std::vector<float> signal;
    for (int i = 0; i < 100; ++i) {
        signal.push_back(std::sin(2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / 20.0f));
    }
    auto result = exco::find_period(signal, 5, 50);
    REQUIRE(result.has_value());
    CHECK(result->period == 20);
    CHECK(result->strength > 0.8f);
}

TEST_CASE("find_period — returns nullopt for flat signal") {
    std::vector<float> flat(100, 1.0f);
    auto result = exco::find_period(flat, 5, 50);
    CHECK_FALSE(result.has_value());
}
```

- [ ] **Step 2: Add test to CMake**

Append `tests/test_signal.cpp` to the `exco_tests` sources in `cpp/CMakeLists.txt`.

- [ ] **Step 3: Run — verify compile fails**

```bash
cd build && cmake .. && cmake --build .
```

Expected: errors about undefined exco::smooth, exco::autocorrelate, exco::find_period.

- [ ] **Step 4: Implement signal.hpp (declarations)**

`cpp/include/exco/signal.hpp`:
```cpp
#pragma once
#include <vector>
#include <optional>

namespace exco {

struct PeriodResult {
    int period;       // frames per cycle
    float strength;   // autocorrelation peak value (0..1)
};

std::vector<float> smooth(const std::vector<float>& signal, int window);

std::vector<float> autocorrelate(const std::vector<float>& signal);

std::optional<PeriodResult> find_period(const std::vector<float>& signal,
                                        int min_period, int max_period);

} // namespace exco
```

- [ ] **Step 5: Implement signal.cpp**

`cpp/src/signal.cpp`:
```cpp
#include "exco/signal.hpp"
#include <cmath>
#include <numeric>

namespace exco {

std::vector<float> smooth(const std::vector<float>& signal, int window) {
    if (window <= 1 || signal.empty()) return signal;
    std::vector<float> out(signal.size());
    int half = window / 2;
    for (int i = 0; i < static_cast<int>(signal.size()); ++i) {
        int start = std::max(0, i - half);
        int end = std::min(static_cast<int>(signal.size()), i + half + 1);
        float sum = 0.0f;
        for (int j = start; j < end; ++j) {
            sum += signal[static_cast<size_t>(j)];
        }
        out[static_cast<size_t>(i)] = sum / static_cast<float>(end - start);
    }
    return out;
}

std::vector<float> autocorrelate(const std::vector<float>& signal) {
    int n = static_cast<int>(signal.size());
    if (n == 0) return {};
    float mean = std::accumulate(signal.begin(), signal.end(), 0.0f) / static_cast<float>(n);
    float variance = 0.0f;
    for (float v : signal) {
        variance += (v - mean) * (v - mean);
    }
    if (variance < 1e-10f) {
        return std::vector<float>(signal.size(), 0.0f);
    }
    std::vector<float> result(signal.size());
    for (int lag = 0; lag < n; ++lag) {
        float sum = 0.0f;
        for (int i = 0; i < n - lag; ++i) {
            sum += (signal[static_cast<size_t>(i)] - mean) *
                   (signal[static_cast<size_t>(i + lag)] - mean);
        }
        result[static_cast<size_t>(lag)] = sum / variance;
    }
    return result;
}

std::optional<PeriodResult> find_period(const std::vector<float>& signal,
                                        int min_period, int max_period) {
    auto acorr = autocorrelate(signal);
    int n = static_cast<int>(acorr.size());
    if (n <= min_period) return std::nullopt;

    int best_lag = -1;
    float best_val = 0.0f;
    int limit = std::min(max_period + 1, n);

    for (int lag = min_period; lag < limit; ++lag) {
        if (acorr[static_cast<size_t>(lag)] > best_val) {
            best_val = acorr[static_cast<size_t>(lag)];
            best_lag = lag;
        }
    }

    if (best_lag < 0 || best_val < 0.3f) return std::nullopt;

    return PeriodResult{best_lag, best_val};
}

} // namespace exco
```

- [ ] **Step 6: Run tests — verify they pass**

```bash
cd build && cmake --build . && ./cpp/exco_tests
```

Expected: all signal tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/include/exco/signal.hpp cpp/src/signal.cpp cpp/tests/test_signal.cpp cpp/CMakeLists.txt
git commit -m "feat(core): signal processing — smooth, autocorrelate, find_period"
```

---

## Task 4: C++ Pattern Extraction & DTW

**Files:**
- Modify: `cpp/include/exco/pattern.hpp`
- Modify: `cpp/src/pattern.cpp`
- Create: `cpp/tests/test_pattern.cpp`

- [ ] **Step 1: Write failing tests**

`cpp/tests/test_pattern.cpp`:
```cpp
#include "doctest.h"
#include "exco/pattern.hpp"
#include <cmath>
#include <vector>

TEST_CASE("dtw_distance — identical sequences = 0") {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    float d = exco::dtw_distance(a, a);
    CHECK(d == doctest::Approx(0.0f));
}

TEST_CASE("dtw_distance — similar sequences close to 0") {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    std::vector<float> b = {1.1f, 2.1f, 3.1f, 2.1f, 1.1f};
    float d = exco::dtw_distance(a, b);
    CHECK(d < 1.0f);
}

TEST_CASE("dtw_distance — different sequences have large distance") {
    std::vector<float> a = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> b = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    float d = exco::dtw_distance(a, b);
    CHECK(d > 5.0f);
}

TEST_CASE("extract_cycle — extracts one period from periodic signal") {
    // 3 cycles of a triangle wave with period 10
    std::vector<float> signal;
    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < 5; ++i) signal.push_back(static_cast<float>(i));
        for (int i = 4; i >= 0; --i) signal.push_back(static_cast<float>(i));
    }
    auto cycle = exco::extract_cycle(signal, 10);
    REQUIRE(cycle.size() == 10);
    // The extracted cycle should match one period of the triangle
    CHECK(cycle[0] == doctest::Approx(0.0f).epsilon(0.5));
    CHECK(cycle[4] == doctest::Approx(4.0f).epsilon(0.5));
}

TEST_CASE("Pattern serialization round-trip") {
    exco::Pattern p;
    p.id = 0;
    p.period_frames = 20;
    p.signature = {1.0f, 2.0f, 3.0f};
    p.dominant_joints = {11, 13, 15};

    auto bytes = exco::serialize_pattern(p);
    auto restored = exco::deserialize_pattern(bytes);
    CHECK(restored.period_frames == 20);
    CHECK(restored.signature.size() == 3);
    CHECK(restored.dominant_joints.size() == 3);
    CHECK(restored.signature[1] == doctest::Approx(2.0f));
    CHECK(restored.dominant_joints[2] == 15);
}

TEST_CASE("PatternMatcher — finds matching pattern") {
    exco::PatternMatcher matcher(2.0f); // dtw threshold
    exco::Pattern p;
    p.id = 1;
    p.period_frames = 5;
    p.signature = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    p.dominant_joints = {0};
    matcher.add_pattern(p);

    std::vector<float> query = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    auto match = matcher.find_match(query);
    REQUIRE(match.has_value());
    CHECK(match->id == 1);
}

TEST_CASE("PatternMatcher — no match for different pattern") {
    exco::PatternMatcher matcher(2.0f);
    exco::Pattern p;
    p.id = 1;
    p.period_frames = 5;
    p.signature = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    p.dominant_joints = {0};
    matcher.add_pattern(p);

    std::vector<float> query = {10.0f, 0.0f, 10.0f, 0.0f, 10.0f};
    auto match = matcher.find_match(query);
    CHECK_FALSE(match.has_value());
}
```

- [ ] **Step 2: Add test to CMake**

Append `tests/test_pattern.cpp` to `exco_tests` sources.

- [ ] **Step 3: Run — verify compile fails**

- [ ] **Step 4: Implement pattern.hpp (declarations)**

`cpp/include/exco/pattern.hpp`:
```cpp
#pragma once
#include <vector>
#include <optional>
#include <cstdint>

namespace exco {

struct Pattern {
    int id;
    int period_frames;
    std::vector<float> signature;
    std::vector<int> dominant_joints;
};

float dtw_distance(const std::vector<float>& a, const std::vector<float>& b);

std::vector<float> extract_cycle(const std::vector<float>& signal, int period);

std::vector<uint8_t> serialize_pattern(const Pattern& p);
Pattern deserialize_pattern(const std::vector<uint8_t>& data);

class PatternMatcher {
public:
    explicit PatternMatcher(float dtw_threshold);
    void add_pattern(const Pattern& p);
    std::optional<Pattern> find_match(const std::vector<float>& cycle) const;
    const std::vector<Pattern>& patterns() const;

private:
    float dtw_threshold_;
    std::vector<Pattern> patterns_;
};

} // namespace exco
```

- [ ] **Step 5: Implement pattern.cpp**

`cpp/src/pattern.cpp`:
```cpp
#include "exco/pattern.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace exco {

float dtw_distance(const std::vector<float>& a, const std::vector<float>& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    if (n == 0 || m == 0) return 0.0f;

    // Full DTW matrix — acceptable for short signatures (~20-60 samples)
    std::vector<std::vector<float>> dp(
        static_cast<size_t>(n + 1),
        std::vector<float>(static_cast<size_t>(m + 1), 1e30f));
    dp[0][0] = 0.0f;

    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            float cost = std::abs(a[static_cast<size_t>(i - 1)] -
                                  b[static_cast<size_t>(j - 1)]);
            dp[static_cast<size_t>(i)][static_cast<size_t>(j)] = cost +
                std::min({dp[static_cast<size_t>(i - 1)][static_cast<size_t>(j)],
                          dp[static_cast<size_t>(i)][static_cast<size_t>(j - 1)],
                          dp[static_cast<size_t>(i - 1)][static_cast<size_t>(j - 1)]});
        }
    }
    return dp[static_cast<size_t>(n)][static_cast<size_t>(m)] / static_cast<float>(std::max(n, m));
}

std::vector<float> extract_cycle(const std::vector<float>& signal, int period) {
    if (period <= 0 || signal.empty()) return {};
    int num_cycles = static_cast<int>(signal.size()) / period;
    if (num_cycles == 0) {
        return std::vector<float>(signal.begin(),
                                  signal.begin() + std::min(period, static_cast<int>(signal.size())));
    }
    // Average across all complete cycles
    std::vector<float> avg(static_cast<size_t>(period), 0.0f);
    for (int c = 0; c < num_cycles; ++c) {
        for (int i = 0; i < period; ++i) {
            avg[static_cast<size_t>(i)] +=
                signal[static_cast<size_t>(c * period + i)];
        }
    }
    for (int i = 0; i < period; ++i) {
        avg[static_cast<size_t>(i)] /= static_cast<float>(num_cycles);
    }
    return avg;
}

std::vector<uint8_t> serialize_pattern(const Pattern& p) {
    std::vector<uint8_t> data;
    auto push = [&](const void* ptr, size_t sz) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(ptr);
        data.insert(data.end(), bytes, bytes + sz);
    };
    int32_t period = p.period_frames;
    push(&period, sizeof(period));

    int32_t sig_len = static_cast<int32_t>(p.signature.size());
    push(&sig_len, sizeof(sig_len));
    push(p.signature.data(), static_cast<size_t>(sig_len) * sizeof(float));

    int32_t joints_len = static_cast<int32_t>(p.dominant_joints.size());
    push(&joints_len, sizeof(joints_len));
    push(p.dominant_joints.data(), static_cast<size_t>(joints_len) * sizeof(int));

    return data;
}

Pattern deserialize_pattern(const std::vector<uint8_t>& data) {
    Pattern p{};
    size_t offset = 0;
    auto pull = [&](void* dst, size_t sz) {
        std::memcpy(dst, data.data() + offset, sz);
        offset += sz;
    };

    int32_t period = 0;
    pull(&period, sizeof(period));
    p.period_frames = period;

    int32_t sig_len = 0;
    pull(&sig_len, sizeof(sig_len));
    p.signature.resize(static_cast<size_t>(sig_len));
    pull(p.signature.data(), static_cast<size_t>(sig_len) * sizeof(float));

    int32_t joints_len = 0;
    pull(&joints_len, sizeof(joints_len));
    p.dominant_joints.resize(static_cast<size_t>(joints_len));
    pull(p.dominant_joints.data(), static_cast<size_t>(joints_len) * sizeof(int));

    return p;
}

PatternMatcher::PatternMatcher(float dtw_threshold)
    : dtw_threshold_(dtw_threshold) {}

void PatternMatcher::add_pattern(const Pattern& p) {
    patterns_.push_back(p);
}

std::optional<Pattern> PatternMatcher::find_match(const std::vector<float>& cycle) const {
    float best_dist = dtw_threshold_;
    const Pattern* best = nullptr;
    for (const auto& p : patterns_) {
        float d = dtw_distance(cycle, p.signature);
        if (d < best_dist) {
            best_dist = d;
            best = &p;
        }
    }
    if (best != nullptr) return *best;
    return std::nullopt;
}

const std::vector<Pattern>& PatternMatcher::patterns() const {
    return patterns_;
}

} // namespace exco
```

- [ ] **Step 6: Run tests — verify they pass**

```bash
cd build && cmake --build . && ./cpp/exco_tests
```

Expected: all pattern tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/include/exco/pattern.hpp cpp/src/pattern.cpp cpp/tests/test_pattern.cpp cpp/CMakeLists.txt
git commit -m "feat(core): pattern extraction, DTW comparison, serialization"
```

---

## Task 5: C++ Rep Counter (Schmitt Trigger)

**Files:**
- Modify: `cpp/include/exco/counter.hpp`
- Modify: `cpp/src/counter.cpp`
- Create: `cpp/tests/test_counter.cpp`

- [ ] **Step 1: Write failing tests**

`cpp/tests/test_counter.cpp`:
```cpp
#include "doctest.h"
#include "exco/counter.hpp"
#include <cmath>

TEST_CASE("RepCounter — counts peaks in sine wave") {
    exco::RepCounter counter(0.3f, 0.7f, 2);
    // Feed 3 complete cycles of 0..1..0
    int last_count = 0;
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 20; ++i) {
            float t = static_cast<float>(i) / 19.0f;
            float val = 0.5f + 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * t);
            counter.push(val);
        }
    }
    CHECK(counter.count() == 3);
}

TEST_CASE("RepCounter — flat signal = 0 counts") {
    exco::RepCounter counter(0.3f, 0.7f, 2);
    for (int i = 0; i < 100; ++i) {
        counter.push(0.5f);
    }
    CHECK(counter.count() == 0);
}

TEST_CASE("RepCounter — reset clears state") {
    exco::RepCounter counter(0.3f, 0.7f, 2);
    for (int i = 0; i < 20; ++i) {
        float val = static_cast<float>(i % 2);
        counter.push(val);
    }
    counter.reset();
    CHECK(counter.count() == 0);
}

TEST_CASE("RepCounter — min_frames_in_state filters jitter") {
    exco::RepCounter counter(0.3f, 0.7f, 5);
    // Quick jitter: should not count
    counter.push(0.0f);
    counter.push(1.0f);
    counter.push(0.0f);
    counter.push(1.0f);
    CHECK(counter.count() == 0);
}
```

- [ ] **Step 2: Add test to CMake**

Append `tests/test_counter.cpp` to `exco_tests` sources.

- [ ] **Step 3: Run — verify compile fails**

- [ ] **Step 4: Implement counter.hpp**

`cpp/include/exco/counter.hpp`:
```cpp
#pragma once

namespace exco {

class RepCounter {
public:
    RepCounter(float down_threshold, float up_threshold, int min_frames_in_state);

    void push(float value);
    int count() const;
    void reset();

private:
    enum class State { IDLE, DOWN, UP };

    float down_threshold_;
    float up_threshold_;
    int min_frames_;
    State state_;
    int frames_in_state_;
    int count_;
};

} // namespace exco
```

- [ ] **Step 5: Implement counter.cpp**

`cpp/src/counter.cpp`:
```cpp
#include "exco/counter.hpp"

namespace exco {

RepCounter::RepCounter(float down_threshold, float up_threshold, int min_frames_in_state)
    : down_threshold_(down_threshold)
    , up_threshold_(up_threshold)
    , min_frames_(min_frames_in_state)
    , state_(State::IDLE)
    , frames_in_state_(0)
    , count_(0) {}

void RepCounter::push(float value) {
    switch (state_) {
    case State::IDLE:
        if (value >= up_threshold_) {
            state_ = State::UP;
            frames_in_state_ = 1;
        } else if (value <= down_threshold_) {
            state_ = State::DOWN;
            frames_in_state_ = 1;
        }
        break;

    case State::UP:
        if (value >= up_threshold_) {
            ++frames_in_state_;
        } else if (value <= down_threshold_ && frames_in_state_ >= min_frames_) {
            state_ = State::DOWN;
            frames_in_state_ = 1;
        } else if (value <= down_threshold_) {
            state_ = State::IDLE;
            frames_in_state_ = 0;
        }
        break;

    case State::DOWN:
        if (value <= down_threshold_) {
            ++frames_in_state_;
        } else if (value >= up_threshold_ && frames_in_state_ >= min_frames_) {
            state_ = State::UP;
            frames_in_state_ = 1;
            ++count_;
        } else if (value >= up_threshold_) {
            state_ = State::IDLE;
            frames_in_state_ = 0;
        }
        break;
    }
}

int RepCounter::count() const {
    return count_;
}

void RepCounter::reset() {
    state_ = State::IDLE;
    frames_in_state_ = 0;
    count_ = 0;
}

} // namespace exco
```

- [ ] **Step 6: Run tests — verify they pass**

```bash
cd build && cmake --build . && ./cpp/exco_tests
```

- [ ] **Step 7: Commit**

```bash
git add cpp/include/exco/counter.hpp cpp/src/counter.cpp cpp/tests/test_counter.cpp cpp/CMakeLists.txt
git commit -m "feat(core): RepCounter — Schmitt trigger state machine"
```

---

## Task 6: C++ AnalyzerCore — Orchestration

**Files:**
- Modify: `cpp/include/exco/analyzer_core.hpp`
- Modify: `cpp/src/analyzer_core.cpp`

This module combines signal, pattern, and counter into one callable unit for the Python side.

- [ ] **Step 1: Implement analyzer_core.hpp**

`cpp/include/exco/analyzer_core.hpp`:
```cpp
#pragma once
#include "exco/signal.hpp"
#include "exco/pattern.hpp"
#include "exco/counter.hpp"
#include <vector>
#include <optional>
#include <unordered_map>

namespace exco {

struct Landmark {
    float x, y, z, visibility;
};

struct AnalysisEvent {
    int pattern_id;   // -1 = new pattern detected
    int count;
    int period_frames;
    std::vector<float> signature;
    std::vector<int> dominant_joints;
};

struct AnalyzerConfig {
    int window_frames = 90;        // ~3 sec at 30 fps
    int min_period = 10;           // shortest allowed cycle
    int max_period = 90;           // longest allowed cycle
    float period_strength = 0.4f;  // autocorrelation threshold
    float dtw_threshold = 2.0f;    // max DTW distance for match
    float counter_down = 0.3f;
    float counter_up = 0.7f;
    int counter_min_frames = 3;
    int smooth_window = 5;
    int num_joints = 33;
};

class AnalyzerCore {
public:
    explicit AnalyzerCore(AnalyzerConfig config = {});

    // Feed one frame of landmarks (33 landmarks per frame).
    // Returns event if a rep was counted or new pattern detected.
    std::optional<AnalysisEvent> push_frame(const std::vector<Landmark>& landmarks);

    // Load previously saved patterns (from DB on startup)
    void load_pattern(const Pattern& p);

    const AnalyzerConfig& config() const;

private:
    AnalyzerConfig config_;
    PatternMatcher matcher_;

    // Per-joint signal buffers (ring buffer style via deque-like vector)
    std::vector<std::vector<float>> joint_signals_;  // [joint_id][frame]
    int frame_count_;

    // Per-pattern counters
    std::unordered_map<int, RepCounter> counters_;
    int next_pattern_id_;

    // Find which joints have highest variance
    std::vector<int> find_dominant_joints(int top_n) const;

    // Build composite signal from dominant joints
    std::vector<float> build_composite(const std::vector<int>& joints) const;
};

} // namespace exco
```

- [ ] **Step 2: Implement analyzer_core.cpp**

`cpp/src/analyzer_core.cpp`:
```cpp
#include "exco/analyzer_core.hpp"
#include "exco/geometry.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace exco {

AnalyzerCore::AnalyzerCore(AnalyzerConfig config)
    : config_(config)
    , matcher_(config.dtw_threshold)
    , joint_signals_(static_cast<size_t>(config.num_joints))
    , frame_count_(0)
    , next_pattern_id_(1) {}

void AnalyzerCore::load_pattern(const Pattern& p) {
    matcher_.add_pattern(p);
    counters_.emplace(p.id, RepCounter(config_.counter_down,
                                        config_.counter_up,
                                        config_.counter_min_frames));
    if (p.id >= next_pattern_id_) {
        next_pattern_id_ = p.id + 1;
    }
}

const AnalyzerConfig& AnalyzerCore::config() const {
    return config_;
}

std::vector<int> AnalyzerCore::find_dominant_joints(int top_n) const {
    std::vector<std::pair<float, int>> variances;
    for (int j = 0; j < config_.num_joints; ++j) {
        const auto& sig = joint_signals_[static_cast<size_t>(j)];
        if (sig.size() < 2) {
            variances.push_back({0.0f, j});
            continue;
        }
        float mean = std::accumulate(sig.begin(), sig.end(), 0.0f) /
                     static_cast<float>(sig.size());
        float var = 0.0f;
        for (float v : sig) {
            var += (v - mean) * (v - mean);
        }
        var /= static_cast<float>(sig.size());
        variances.push_back({var, j});
    }
    std::sort(variances.begin(), variances.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<int> result;
    for (int i = 0; i < std::min(top_n, static_cast<int>(variances.size())); ++i) {
        if (variances[static_cast<size_t>(i)].first > 1e-6f) {
            result.push_back(variances[static_cast<size_t>(i)].second);
        }
    }
    return result;
}

std::vector<float> AnalyzerCore::build_composite(const std::vector<int>& joints) const {
    if (joints.empty()) return {};
    size_t len = joint_signals_[static_cast<size_t>(joints[0])].size();
    std::vector<float> composite(len, 0.0f);
    for (int j : joints) {
        const auto& sig = joint_signals_[static_cast<size_t>(j)];
        for (size_t i = 0; i < len; ++i) {
            composite[i] += sig[i];
        }
    }
    float scale = 1.0f / static_cast<float>(joints.size());
    for (float& v : composite) v *= scale;
    return composite;
}

std::optional<AnalysisEvent> AnalyzerCore::push_frame(
        const std::vector<Landmark>& landmarks) {
    if (static_cast<int>(landmarks.size()) != config_.num_joints) {
        return std::nullopt;
    }

    // Append y-coordinate of each joint to signal buffers
    // (y captures most exercise motion — up/down)
    for (int j = 0; j < config_.num_joints; ++j) {
        auto& sig = joint_signals_[static_cast<size_t>(j)];
        sig.push_back(landmarks[static_cast<size_t>(j)].y);
        // Keep only window_frames
        if (static_cast<int>(sig.size()) > config_.window_frames) {
            sig.erase(sig.begin());
        }
    }
    ++frame_count_;

    // Need at least window_frames to analyze
    if (static_cast<int>(joint_signals_[0].size()) < config_.window_frames) {
        return std::nullopt;
    }

    // Find dominant joints
    auto dominant = find_dominant_joints(5);
    if (dominant.empty()) return std::nullopt;

    // Build composite signal and smooth
    auto composite = build_composite(dominant);
    composite = smooth(composite, config_.smooth_window);

    // Find periodicity
    auto period_result = find_period(composite, config_.min_period, config_.max_period);
    if (!period_result.has_value()) return std::nullopt;

    int period = period_result->period;

    // Extract one cycle
    auto cycle = extract_cycle(composite, period);
    if (cycle.empty()) return std::nullopt;

    // Normalize cycle to 0..1
    float cmin = *std::min_element(cycle.begin(), cycle.end());
    float cmax = *std::max_element(cycle.begin(), cycle.end());
    if (cmax - cmin > 1e-6f) {
        for (float& v : cycle) {
            v = (v - cmin) / (cmax - cmin);
        }
    }

    // Match against known patterns
    auto match = matcher_.find_match(cycle);

    // Normalize current composite value to 0..1 for counter
    float current = composite.back();
    float smin = *std::min_element(composite.begin(), composite.end());
    float smax = *std::max_element(composite.begin(), composite.end());
    float normalized = (smax - smin > 1e-6f)
        ? (current - smin) / (smax - smin)
        : 0.5f;

    if (match.has_value()) {
        // Known pattern — feed counter
        auto it = counters_.find(match->id);
        if (it == counters_.end()) {
            counters_.emplace(match->id, RepCounter(config_.counter_down,
                                                     config_.counter_up,
                                                     config_.counter_min_frames));
            it = counters_.find(match->id);
        }
        int prev = it->second.count();
        it->second.push(normalized);
        if (it->second.count() > prev) {
            return AnalysisEvent{match->id, it->second.count(),
                                 period, {}, {}};
        }
    } else {
        // New pattern
        Pattern new_p;
        new_p.id = next_pattern_id_++;
        new_p.period_frames = period;
        new_p.signature = cycle;
        new_p.dominant_joints = dominant;
        matcher_.add_pattern(new_p);
        counters_.emplace(new_p.id, RepCounter(config_.counter_down,
                                                config_.counter_up,
                                                config_.counter_min_frames));
        return AnalysisEvent{-1, 0, period, cycle, dominant};
    }

    return std::nullopt;
}

} // namespace exco
```

- [ ] **Step 3: Build and verify compiles**

```bash
cd build && cmake --build .
```

- [ ] **Step 4: Commit**

```bash
git add cpp/include/exco/analyzer_core.hpp cpp/src/analyzer_core.cpp
git commit -m "feat(core): AnalyzerCore — orchestrates signal→pattern→counter"
```

---

## Task 7: pybind11 Bindings

**Files:**
- Modify: `cpp/bindings/python_bindings.cpp`

- [ ] **Step 1: Implement full bindings**

`cpp/bindings/python_bindings.cpp`:
```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/optional.h>

#include "exco/geometry.hpp"
#include "exco/signal.hpp"
#include "exco/pattern.hpp"
#include "exco/counter.hpp"
#include "exco/analyzer_core.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_exco_cpp, m) {
    m.doc() = "Exercise counter C++ core";

    // geometry
    m.def("distance_2d", &exco::distance_2d);
    m.def("angle_between", &exco::angle_between);
    m.def("normalize", &exco::normalize);

    // signal
    py::class_<exco::PeriodResult>(m, "PeriodResult")
        .def_readonly("period", &exco::PeriodResult::period)
        .def_readonly("strength", &exco::PeriodResult::strength);
    m.def("smooth", &exco::smooth);
    m.def("autocorrelate", &exco::autocorrelate);
    m.def("find_period", &exco::find_period);

    // pattern
    py::class_<exco::Pattern>(m, "Pattern")
        .def(py::init<>())
        .def_readwrite("id", &exco::Pattern::id)
        .def_readwrite("period_frames", &exco::Pattern::period_frames)
        .def_readwrite("signature", &exco::Pattern::signature)
        .def_readwrite("dominant_joints", &exco::Pattern::dominant_joints);
    m.def("dtw_distance", &exco::dtw_distance);
    m.def("extract_cycle", &exco::extract_cycle);
    m.def("serialize_pattern", &exco::serialize_pattern);
    m.def("deserialize_pattern", &exco::deserialize_pattern);

    // counter
    py::class_<exco::RepCounter>(m, "RepCounter")
        .def(py::init<float, float, int>())
        .def("push", &exco::RepCounter::push)
        .def("count", &exco::RepCounter::count)
        .def("reset", &exco::RepCounter::reset);

    // landmark
    py::class_<exco::Landmark>(m, "Landmark")
        .def(py::init<>())
        .def_readwrite("x", &exco::Landmark::x)
        .def_readwrite("y", &exco::Landmark::y)
        .def_readwrite("z", &exco::Landmark::z)
        .def_readwrite("visibility", &exco::Landmark::visibility);

    // analysis event
    py::class_<exco::AnalysisEvent>(m, "AnalysisEvent")
        .def_readonly("pattern_id", &exco::AnalysisEvent::pattern_id)
        .def_readonly("count", &exco::AnalysisEvent::count)
        .def_readonly("period_frames", &exco::AnalysisEvent::period_frames)
        .def_readonly("signature", &exco::AnalysisEvent::signature)
        .def_readonly("dominant_joints", &exco::AnalysisEvent::dominant_joints);

    // config
    py::class_<exco::AnalyzerConfig>(m, "AnalyzerConfig")
        .def(py::init<>())
        .def_readwrite("window_frames", &exco::AnalyzerConfig::window_frames)
        .def_readwrite("min_period", &exco::AnalyzerConfig::min_period)
        .def_readwrite("max_period", &exco::AnalyzerConfig::max_period)
        .def_readwrite("period_strength", &exco::AnalyzerConfig::period_strength)
        .def_readwrite("dtw_threshold", &exco::AnalyzerConfig::dtw_threshold)
        .def_readwrite("counter_down", &exco::AnalyzerConfig::counter_down)
        .def_readwrite("counter_up", &exco::AnalyzerConfig::counter_up)
        .def_readwrite("counter_min_frames", &exco::AnalyzerConfig::counter_min_frames)
        .def_readwrite("smooth_window", &exco::AnalyzerConfig::smooth_window)
        .def_readwrite("num_joints", &exco::AnalyzerConfig::num_joints);

    // analyzer core
    py::class_<exco::AnalyzerCore>(m, "AnalyzerCore")
        .def(py::init<exco::AnalyzerConfig>(), py::arg("config") = exco::AnalyzerConfig{})
        .def("push_frame", &exco::AnalyzerCore::push_frame)
        .def("load_pattern", &exco::AnalyzerCore::load_pattern)
        .def("config", &exco::AnalyzerCore::config);
}
```

- [ ] **Step 2: Build with pybind11**

```bash
cd /home/dchuprina/exercises-counter
pip install pybind11 scikit-build-core
pip install -e .
python -c "import exco._exco_cpp as cpp; print(dir(cpp))"
```

Expected: list of bound names.

- [ ] **Step 3: Commit**

```bash
git add cpp/bindings/python_bindings.cpp
git commit -m "feat: pybind11 bindings for full C++ core"
```

---

## Task 8: Python Events & DB Layer

**Files:**
- Create: `python/exco/events.py`
- Create: `python/exco/db.py`
- Create: `tests/test_db.py`

- [ ] **Step 1: Write events.py**

`python/exco/events.py`:
```python
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class ExerciseEvent:
    pattern_id: int
    count: int
    timestamp: float
```

- [ ] **Step 2: Write failing test for DB**

`tests/test_db.py`:
```python
import tempfile
import os
import pytest
from exco.db import ExcoDB


def test_create_tables():
    with tempfile.TemporaryDirectory() as d:
        db = ExcoDB(os.path.join(d, "test.db"))
        # Should not raise
        db.close()


def test_write_and_read_landmarks():
    with tempfile.TemporaryDirectory() as d:
        db = ExcoDB(os.path.join(d, "test.db"))
        landmarks = [
            (0.1, 0, i, float(i) / 33, float(i) / 33, 0.0, 0.9)
            for i in range(33)
        ]
        db.write_landmarks(landmarks)
        rows = db.read_landmarks_since(0, 0.0)
        assert len(rows) == 33
        db.close()


def test_write_and_read_pattern():
    with tempfile.TemporaryDirectory() as d:
        db = ExcoDB(os.path.join(d, "test.db"))
        pid = db.write_pattern(b"\x01\x02\x03", 20, "[11,13]")
        patterns = db.read_patterns()
        assert len(patterns) == 1
        assert patterns[0]["id"] == pid
        assert patterns[0]["signature"] == b"\x01\x02\x03"
        db.close()


def test_write_and_read_events():
    with tempfile.TemporaryDirectory() as d:
        db = ExcoDB(os.path.join(d, "test.db"))
        pid = db.write_pattern(b"\x01", 10, "[]")
        db.write_event(pid, 1, 0.5)
        db.write_event(pid, 2, 1.0)
        events = db.read_events_since(0)
        assert len(events) == 2
        assert events[1]["count"] == 2
        db.close()
```

- [ ] **Step 3: Run test — verify it fails**

```bash
cd /home/dchuprina/exercises-counter
python -m pytest tests/test_db.py -v
```

Expected: ImportError (exco.db doesn't exist).

- [ ] **Step 4: Implement db.py**

`python/exco/db.py`:
```python
from __future__ import annotations

import sqlite3
import threading
from typing import Any


class ExcoDB:
    def __init__(self, path: str) -> None:
        self._path = path
        self._local = threading.local()
        self._init_tables()

    @property
    def _conn(self) -> sqlite3.Connection:
        if not hasattr(self._local, "conn"):
            conn = sqlite3.connect(self._path, timeout=5.0)
            conn.execute("PRAGMA journal_mode=WAL")
            conn.execute("PRAGMA synchronous=NORMAL")
            conn.row_factory = sqlite3.Row
            self._local.conn = conn
        return self._local.conn

    def _init_tables(self) -> None:
        c = self._conn
        c.executescript("""
            CREATE TABLE IF NOT EXISTS landmarks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp REAL NOT NULL,
                frame_id INTEGER NOT NULL,
                joint_id INTEGER NOT NULL,
                x REAL NOT NULL,
                y REAL NOT NULL,
                z REAL NOT NULL,
                visibility REAL NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_landmarks_frame
                ON landmarks(frame_id);
            CREATE INDEX IF NOT EXISTS idx_landmarks_timestamp
                ON landmarks(timestamp);

            CREATE TABLE IF NOT EXISTS patterns (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                signature BLOB NOT NULL,
                period_frames INTEGER NOT NULL,
                dominant_joints TEXT NOT NULL,
                created_at REAL NOT NULL DEFAULT (strftime('%s','now'))
            );

            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                pattern_id INTEGER NOT NULL REFERENCES patterns(id),
                count INTEGER NOT NULL,
                timestamp REAL NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_events_timestamp
                ON events(timestamp);
        """)

    def write_landmarks(
        self, rows: list[tuple[float, int, int, float, float, float, float]]
    ) -> None:
        self._conn.executemany(
            "INSERT INTO landmarks (timestamp, frame_id, joint_id, x, y, z, visibility) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            rows,
        )
        self._conn.commit()

    def read_landmarks_since(
        self, min_frame_id: int, min_timestamp: float
    ) -> list[dict[str, Any]]:
        cur = self._conn.execute(
            "SELECT * FROM landmarks WHERE frame_id >= ? AND timestamp >= ? "
            "ORDER BY frame_id, joint_id",
            (min_frame_id, min_timestamp),
        )
        return [dict(row) for row in cur.fetchall()]

    def write_pattern(
        self, signature: bytes, period_frames: int, dominant_joints: str
    ) -> int:
        cur = self._conn.execute(
            "INSERT INTO patterns (signature, period_frames, dominant_joints) "
            "VALUES (?, ?, ?)",
            (signature, period_frames, dominant_joints),
        )
        self._conn.commit()
        return cur.lastrowid  # type: ignore[return-value]

    def read_patterns(self) -> list[dict[str, Any]]:
        cur = self._conn.execute("SELECT * FROM patterns ORDER BY id")
        return [dict(row) for row in cur.fetchall()]

    def write_event(self, pattern_id: int, count: int, timestamp: float) -> None:
        self._conn.execute(
            "INSERT INTO events (pattern_id, count, timestamp) VALUES (?, ?, ?)",
            (pattern_id, count, timestamp),
        )
        self._conn.commit()

    def read_events_since(self, min_id: int) -> list[dict[str, Any]]:
        cur = self._conn.execute(
            "SELECT * FROM events WHERE id > ? ORDER BY id", (min_id,)
        )
        return [dict(row) for row in cur.fetchall()]

    def close(self) -> None:
        if hasattr(self._local, "conn"):
            self._local.conn.close()
```

- [ ] **Step 5: Run tests — verify they pass**

```bash
python -m pytest tests/test_db.py -v
```

Expected: 4 tests pass.

- [ ] **Step 6: Commit**

```bash
git add python/exco/events.py python/exco/db.py tests/test_db.py
git commit -m "feat: ExcoDB — SQLite layer with WAL, events dataclass"
```

---

## Task 9: Python Pose Detection Layer

**Files:**
- Create: `python/exco/pose/__init__.py`
- Create: `python/exco/pose/base.py`
- Create: `python/exco/pose/mediapipe_backend.py`

- [ ] **Step 1: Create Protocol**

`python/exco/pose/__init__.py`:
```python
from exco.pose.base import Landmark, PoseDetector
from exco.pose.mediapipe_backend import MediaPipeBackend

__all__ = ["Landmark", "PoseDetector", "MediaPipeBackend"]
```

`python/exco/pose/base.py`:
```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

import numpy as np
from numpy.typing import NDArray


@dataclass(frozen=True)
class Landmark:
    x: float
    y: float
    z: float
    visibility: float


class PoseDetector(Protocol):
    def detect(self, frame: NDArray[np.uint8]) -> list[Landmark] | None:
        """Return 33 landmarks or None if no person detected."""
        ...

    def close(self) -> None: ...
```

- [ ] **Step 2: Implement MediaPipe backend**

`python/exco/pose/mediapipe_backend.py`:
```python
from __future__ import annotations

import mediapipe as mp
import numpy as np
from numpy.typing import NDArray

from exco.pose.base import Landmark

_mp_pose = mp.solutions.pose


class MediaPipeBackend:
    def __init__(
        self,
        model_complexity: int = 1,
        min_detection_confidence: float = 0.5,
        min_tracking_confidence: float = 0.5,
    ) -> None:
        self._pose = _mp_pose.Pose(
            static_image_mode=False,
            model_complexity=model_complexity,
            min_detection_confidence=min_detection_confidence,
            min_tracking_confidence=min_tracking_confidence,
        )

    def detect(self, frame: NDArray[np.uint8]) -> list[Landmark] | None:
        result = self._pose.process(frame)
        if result.pose_landmarks is None:
            return None
        return [
            Landmark(x=lm.x, y=lm.y, z=lm.z, visibility=lm.visibility)
            for lm in result.pose_landmarks.landmark
        ]

    def close(self) -> None:
        self._pose.close()
```

- [ ] **Step 3: Commit**

```bash
git add python/exco/pose/
git commit -m "feat: PoseDetector protocol + MediaPipe backend"
```

---

## Task 10: Python Writer Process

**Files:**
- Create: `python/exco/writer.py`

- [ ] **Step 1: Implement Writer**

`python/exco/writer.py`:
```python
from __future__ import annotations

import time
import cv2

from exco.db import ExcoDB
from exco.pose.base import PoseDetector


class LandmarkWriter:
    def __init__(
        self,
        source: str | int,
        db_path: str,
        detector: PoseDetector,
    ) -> None:
        self._source = source
        self._db = ExcoDB(db_path)
        self._detector = detector

    def run(self) -> None:
        cap = cv2.VideoCapture(self._source)
        if not cap.isOpened():
            raise RuntimeError(f"Cannot open video source: {self._source}")

        frame_id = 0
        start_time = time.monotonic()

        try:
            while True:
                ret, frame = cap.read()
                if not ret:
                    break

                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                landmarks = self._detector.detect(rgb)

                if landmarks is not None:
                    ts = time.monotonic() - start_time
                    rows = [
                        (ts, frame_id, j, lm.x, lm.y, lm.z, lm.visibility)
                        for j, lm in enumerate(landmarks)
                    ]
                    self._db.write_landmarks(rows)

                frame_id += 1
        finally:
            cap.release()
            self._detector.close()
            self._db.close()
```

- [ ] **Step 2: Commit**

```bash
git add python/exco/writer.py
git commit -m "feat: LandmarkWriter — video capture → pose → SQLite"
```

---

## Task 11: Python Analyzer Process

**Files:**
- Create: `python/exco/analyzer.py`

- [ ] **Step 1: Implement Analyzer**

`python/exco/analyzer.py`:
```python
from __future__ import annotations

import json
import time

from exco.db import ExcoDB

try:
    import exco._exco_cpp as cpp
except ImportError:
    cpp = None  # type: ignore[assignment]


class PatternAnalyzer:
    def __init__(self, db_path: str, poll_interval: float = 0.1) -> None:
        if cpp is None:
            raise RuntimeError("C++ core not built. Run: pip install -e .")
        self._db = ExcoDB(db_path)
        self._poll = poll_interval
        config = cpp.AnalyzerConfig()
        self._core = cpp.AnalyzerCore(config)
        self._last_frame_id = -1
        self._load_known_patterns()

    def _load_known_patterns(self) -> None:
        for row in self._db.read_patterns():
            p = cpp.Pattern()
            p.id = row["id"]
            p.period_frames = row["period_frames"]
            blob = row["signature"]
            restored = cpp.deserialize_pattern(
                list(blob) if isinstance(blob, (bytes, bytearray)) else blob
            )
            p.signature = restored.signature
            p.dominant_joints = json.loads(row["dominant_joints"])
            self._core.load_pattern(p)

    def run(self) -> None:
        try:
            while True:
                rows = self._db.read_landmarks_since(
                    self._last_frame_id + 1, 0.0
                )
                if not rows:
                    time.sleep(self._poll)
                    continue

                # Group by frame_id
                frames: dict[int, list[dict]] = {}
                for r in rows:
                    fid = r["frame_id"]
                    frames.setdefault(fid, []).append(r)

                for fid in sorted(frames.keys()):
                    frame_rows = sorted(frames[fid], key=lambda r: r["joint_id"])
                    if len(frame_rows) != 33:
                        continue

                    landmarks = []
                    for r in frame_rows:
                        lm = cpp.Landmark()
                        lm.x = r["x"]
                        lm.y = r["y"]
                        lm.z = r["z"]
                        lm.visibility = r["visibility"]
                        landmarks.append(lm)

                    event = self._core.push_frame(landmarks)
                    ts = frame_rows[0]["timestamp"]

                    if event is not None:
                        if event.pattern_id == -1:
                            # New pattern detected — save to DB
                            sig_bytes = bytes(cpp.serialize_pattern(
                                self._make_pattern(event)
                            ))
                            joints_json = json.dumps(list(event.dominant_joints))
                            self._db.write_pattern(
                                sig_bytes, event.period_frames, joints_json
                            )
                        else:
                            # Rep counted
                            self._db.write_event(
                                event.pattern_id, event.count, ts
                            )

                    self._last_frame_id = fid

        except KeyboardInterrupt:
            pass
        finally:
            self._db.close()

    @staticmethod
    def _make_pattern(event: object) -> object:
        p = cpp.Pattern()
        p.id = 0
        p.period_frames = event.period_frames  # type: ignore[attr-defined]
        p.signature = list(event.signature)  # type: ignore[attr-defined]
        p.dominant_joints = list(event.dominant_joints)  # type: ignore[attr-defined]
        return p
```

- [ ] **Step 2: Commit**

```bash
git add python/exco/analyzer.py
git commit -m "feat: PatternAnalyzer — polls DB, detects patterns, counts reps"
```

---

## Task 12: Python __init__.py Re-exports

**Files:**
- Modify: `python/exco/__init__.py`

- [ ] **Step 1: Update __init__.py**

`python/exco/__init__.py`:
```python
"""Exercise counter — automatic repetition detection from video."""

from exco.events import ExerciseEvent
from exco.db import ExcoDB

__all__ = ["ExerciseEvent", "ExcoDB"]
```

- [ ] **Step 2: Commit**

```bash
git add python/exco/__init__.py
git commit -m "feat: re-export public API from exco package"
```

---

## Task 13: Demo CLI

**Files:**
- Create: `demo/__init__.py`
- Create: `demo/cli.py`

- [ ] **Step 1: Implement CLI**

`demo/__init__.py`:
```python
```

`demo/cli.py`:
```python
from __future__ import annotations

import argparse
import multiprocessing
import sys
import time

from exco.db import ExcoDB
from exco.writer import LandmarkWriter
from exco.analyzer import PatternAnalyzer
from exco.pose.mediapipe_backend import MediaPipeBackend


def run_writer(source: str | int, db_path: str) -> None:
    detector = MediaPipeBackend(model_complexity=1)
    writer = LandmarkWriter(source, db_path, detector)
    writer.run()


def run_analyzer(db_path: str) -> None:
    analyzer = PatternAnalyzer(db_path)
    analyzer.run()


def run_monitor(db_path: str) -> None:
    """Print events to stdout as they appear."""
    db = ExcoDB(db_path)
    last_id = 0
    try:
        while True:
            events = db.read_events_since(last_id)
            for e in events:
                print(
                    f"[{e['timestamp']:.2f}s] "
                    f"exercise #{e['pattern_id']}  "
                    f"count: {e['count']}"
                )
                last_id = e["id"]
            time.sleep(0.2)
    except KeyboardInterrupt:
        pass
    finally:
        db.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Exercise counter CLI")
    parser.add_argument("video", nargs="?", help="Path to video file")
    parser.add_argument("--camera", type=int, help="Camera index (e.g. 0)")
    parser.add_argument(
        "--db", default="exercises.db", help="SQLite database path"
    )
    args = parser.parse_args()

    if args.video is None and args.camera is None:
        parser.error("Provide a video file path or --camera N")

    source: str | int = args.video if args.video else args.camera

    writer_proc = multiprocessing.Process(
        target=run_writer, args=(source, args.db)
    )
    analyzer_proc = multiprocessing.Process(
        target=run_analyzer, args=(args.db,)
    )

    writer_proc.start()
    time.sleep(0.5)  # let writer create DB first
    analyzer_proc.start()

    try:
        run_monitor(args.db)
    except KeyboardInterrupt:
        pass
    finally:
        writer_proc.terminate()
        analyzer_proc.terminate()
        writer_proc.join(timeout=3)
        analyzer_proc.join(timeout=3)


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Test CLI runs (smoke test)**

```bash
cd /home/dchuprina/exercises-counter
python -m demo.cli --help
```

Expected: prints usage without errors.

- [ ] **Step 3: Commit**

```bash
git add demo/
git commit -m "feat: CLI demo — Writer + Analyzer + stdout monitor"
```

---

## Task 14: Demo Web Server

**Files:**
- Create: `demo/web/__init__.py`
- Create: `demo/web/server.py`
- Create: `demo/web/static/index.html`

- [ ] **Step 1: Implement FastAPI server**

`demo/web/__init__.py`:
```python
```

`demo/web/server.py`:
```python
from __future__ import annotations

import argparse
import asyncio
import json
import multiprocessing
import time
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

from exco.db import ExcoDB
from exco.writer import LandmarkWriter
from exco.analyzer import PatternAnalyzer
from exco.pose.mediapipe_backend import MediaPipeBackend

STATIC_DIR = Path(__file__).parent / "static"

app = FastAPI(title="Exercise Counter")
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

DB_PATH = "exercises.db"


@app.get("/")
async def index() -> HTMLResponse:
    html = (STATIC_DIR / "index.html").read_text()
    return HTMLResponse(html)


@app.websocket("/ws/events")
async def ws_events(ws: WebSocket) -> None:
    await ws.accept()
    db = ExcoDB(DB_PATH)
    last_id = 0
    try:
        while True:
            events = db.read_events_since(last_id)
            for e in events:
                await ws.send_text(json.dumps({
                    "pattern_id": e["pattern_id"],
                    "count": e["count"],
                    "timestamp": round(e["timestamp"], 2),
                }))
                last_id = e["id"]
            await asyncio.sleep(0.2)
    except WebSocketDisconnect:
        pass
    finally:
        db.close()


def run_writer(source: str | int, db_path: str) -> None:
    detector = MediaPipeBackend(model_complexity=1)
    writer = LandmarkWriter(source, db_path, detector)
    writer.run()


def run_analyzer(db_path: str) -> None:
    analyzer = PatternAnalyzer(db_path)
    analyzer.run()


def main() -> None:
    global DB_PATH
    parser = argparse.ArgumentParser(description="Exercise counter web demo")
    parser.add_argument("video", nargs="?", help="Path to video file")
    parser.add_argument("--camera", type=int, help="Camera index")
    parser.add_argument("--db", default="exercises.db")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    DB_PATH = args.db

    procs: list[multiprocessing.Process] = []
    if args.video or args.camera is not None:
        source: str | int = args.video if args.video else args.camera
        wp = multiprocessing.Process(target=run_writer, args=(source, args.db))
        ap = multiprocessing.Process(target=run_analyzer, args=(args.db,))
        wp.start()
        time.sleep(0.5)
        ap.start()
        procs = [wp, ap]

    try:
        uvicorn.run(app, host=args.host, port=args.port)
    finally:
        for p in procs:
            p.terminate()
            p.join(timeout=3)


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Create index.html**

`demo/web/static/index.html`:
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Exercise Counter</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: monospace; background: #1a1a2e; color: #eee; padding: 2rem; }
        h1 { margin-bottom: 1rem; color: #e94560; }
        #exercises { display: flex; flex-wrap: wrap; gap: 1rem; margin-bottom: 2rem; }
        .exercise-card {
            background: #16213e; border: 2px solid #0f3460;
            border-radius: 8px; padding: 1.5rem; min-width: 200px; text-align: center;
        }
        .exercise-card .count { font-size: 3rem; font-weight: bold; color: #e94560; }
        .exercise-card .label { font-size: 0.9rem; color: #aaa; margin-top: 0.5rem; }
        #log { background: #0f0f23; padding: 1rem; border-radius: 8px;
               max-height: 400px; overflow-y: auto; font-size: 0.85rem; }
        #log p { padding: 2px 0; border-bottom: 1px solid #1a1a2e; }
        #status { margin-bottom: 1rem; color: #aaa; }
        .connected { color: #4ecca3 !important; }
        .disconnected { color: #e94560 !important; }
    </style>
</head>
<body>
    <h1>Exercise Counter</h1>
    <p id="status" class="disconnected">Disconnected</p>
    <div id="exercises"></div>
    <h2 style="margin-bottom: 0.5rem;">Event Log</h2>
    <div id="log"></div>

    <script>
        const exercises = {};
        const exercisesDiv = document.getElementById('exercises');
        const logDiv = document.getElementById('log');
        const statusEl = document.getElementById('status');

        function updateCard(patternId, count) {
            exercises[patternId] = count;
            exercisesDiv.innerHTML = '';
            for (const [pid, cnt] of Object.entries(exercises)) {
                const card = document.createElement('div');
                card.className = 'exercise-card';
                card.innerHTML = `
                    <div class="count">${cnt}</div>
                    <div class="label">Exercise #${pid}</div>
                `;
                exercisesDiv.appendChild(card);
            }
        }

        function addLog(msg) {
            const p = document.createElement('p');
            p.textContent = msg;
            logDiv.prepend(p);
            if (logDiv.children.length > 200) logDiv.lastChild.remove();
        }

        function connect() {
            const ws = new WebSocket(`ws://${location.host}/ws/events`);
            ws.onopen = () => {
                statusEl.textContent = 'Connected';
                statusEl.className = 'connected';
            };
            ws.onclose = () => {
                statusEl.textContent = 'Disconnected — reconnecting...';
                statusEl.className = 'disconnected';
                setTimeout(connect, 2000);
            };
            ws.onmessage = (e) => {
                const data = JSON.parse(e.data);
                updateCard(data.pattern_id, data.count);
                addLog(`[${data.timestamp}s] exercise #${data.pattern_id} — count: ${data.count}`);
            };
        }

        connect();
    </script>
</body>
</html>
```

- [ ] **Step 3: Commit**

```bash
git add demo/web/
git commit -m "feat: web demo — FastAPI + WebSocket dashboard"
```

---

## Task 15: Integration Test with Synthetic Data

**Files:**
- Create: `tests/test_pipeline.py`

- [ ] **Step 1: Write end-to-end test**

`tests/test_pipeline.py`:
```python
"""Integration test: synthetic landmarks → AnalyzerCore → events."""
import math
import pytest

try:
    import exco._exco_cpp as cpp
except ImportError:
    pytest.skip("C++ core not built", allow_module_level=True)


def make_sine_landmarks(frame_idx: int, period: int = 20) -> list:
    """Generate 33 landmarks where joints 11,13 oscillate with given period."""
    t = math.sin(2 * math.pi * frame_idx / period)
    landmarks = []
    for j in range(33):
        lm = cpp.Landmark()
        lm.x = 0.5
        lm.visibility = 1.0
        lm.z = 0.0
        if j in (11, 13):
            lm.y = 0.5 + 0.3 * t  # oscillating joints
        else:
            lm.y = 0.5  # static joints
        landmarks.append(lm)
    return landmarks


def test_analyzer_detects_periodic_pattern():
    config = cpp.AnalyzerConfig()
    config.window_frames = 60
    config.min_period = 10
    config.max_period = 50
    config.period_strength = 0.3
    config.dtw_threshold = 3.0
    config.smooth_window = 3
    config.counter_min_frames = 2

    core = cpp.AnalyzerCore(config)

    events = []
    for i in range(200):
        landmarks = make_sine_landmarks(i, period=20)
        event = core.push_frame(landmarks)
        if event is not None:
            events.append(event)

    # Should have detected at least a new pattern event
    new_patterns = [e for e in events if e.pattern_id == -1]
    assert len(new_patterns) >= 1, "Should detect at least one new pattern"

    # Should have counted some reps
    reps = [e for e in events if e.pattern_id > 0]
    assert len(reps) >= 2, f"Should count reps, got {len(reps)}"


def test_analyzer_no_events_for_static_pose():
    core = cpp.AnalyzerCore()
    for i in range(200):
        landmarks = []
        for j in range(33):
            lm = cpp.Landmark()
            lm.x = 0.5
            lm.y = 0.5
            lm.z = 0.0
            lm.visibility = 1.0
            landmarks.append(lm)
        event = core.push_frame(landmarks)
        assert event is None, "Static pose should not trigger events"
```

- [ ] **Step 2: Run integration test**

```bash
python -m pytest tests/test_pipeline.py -v
```

Expected: both tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_pipeline.py
git commit -m "test: integration test — synthetic landmarks through AnalyzerCore"
```

---

## Task 16: Documentation

**Files:**
- Create: `docs/BUILD.md`
- Create: `docs/ARCHITECTURE.md`
- Create: `docs/TUNING.md`
- Modify: `README.md`

- [ ] **Step 1: Write BUILD.md**

`docs/BUILD.md`:
```markdown
# Build Instructions

## Prerequisites

- Python 3.11+
- CMake 3.20+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- pip

## Quick Start (Linux / macOS)

```bash
# Clone
git clone https://github.com/vGubriienko/exercises-counter.git
cd exercises-counter

# Create venv
python -m venv .venv
source .venv/bin/activate

# Install with C++ core
pip install -e ".[web,dev]"

# Verify
python -c "import exco._exco_cpp; print('OK')"
```

## Raspberry Pi 5

Same as Linux. MediaPipe supports aarch64:

```bash
pip install -e ".[web]"
```

For camera access, ensure the user is in the `video` group:
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

Works natively — CMake detects arm64:

```bash
pip install -e ".[web,dev]"
```
```

- [ ] **Step 2: Write ARCHITECTURE.md**

`docs/ARCHITECTURE.md`:
```markdown
# Architecture

## Overview

Two independent processes communicate through SQLite:

```
Writer (Python)          SQLite DB           Analyzer (Python + C++)
video → MediaPipe    →  landmarks table  →  signal analysis
                        patterns table   ←  pattern detection
                        events table     ←  rep counting
                                                  ↓
                                            Web UI (FastAPI)
```

## Writer Process

Captures video frames, runs MediaPipe Pose, writes 33 landmarks per frame
to SQLite. No analysis — just capture and persist.

Source: `python/exco/writer.py`

## Analyzer Process

Polls SQLite for new landmarks. For each frame:

1. Appends y-coordinate of each joint to sliding window buffers
2. Finds joints with highest variance (dominant joints)
3. Builds composite signal from dominant joints
4. Smooths signal (moving average)
5. Runs autocorrelation to detect periodicity
6. Extracts one cycle as pattern signature
7. Compares against known patterns via DTW
8. If match: feeds Schmitt trigger counter → emits event
9. If no match: saves as new pattern

Source: C++ core in `cpp/`, Python orchestration in `python/exco/analyzer.py`

## C++ Core (namespace `exco`)

Pure math, zero dependencies beyond STL:

- `geometry.hpp` — distance, angle, normalize (header-only)
- `signal.hpp` — smooth, autocorrelate, find_period
- `pattern.hpp` — Pattern struct, DTW, extract_cycle, serialize/deserialize
- `counter.hpp` — RepCounter (Schmitt trigger)
- `analyzer_core.hpp` — AnalyzerCore (orchestrates the pipeline)

## Protocol-Based Pose Detection

`PoseDetector` is a Python Protocol. Default: MediaPipe.
iOS developers can swap for Vision framework by implementing the same interface.

Source: `python/exco/pose/base.py`

## SQLite as IPC

WAL mode enables concurrent reads + writes. Writer commits per frame.
Analyzer polls every 100ms. No message queues, no sockets — just a file.
```

- [ ] **Step 3: Write TUNING.md**

`docs/TUNING.md`:
```markdown
# Tuning Guide

## AnalyzerConfig Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `window_frames` | 90 | Sliding window size (~3s at 30fps) |
| `min_period` | 10 | Minimum cycle length in frames |
| `max_period` | 90 | Maximum cycle length in frames |
| `period_strength` | 0.4 | Autocorrelation threshold (0-1). Lower = more sensitive |
| `dtw_threshold` | 2.0 | Max DTW distance for pattern match. Lower = stricter |
| `counter_down` | 0.3 | Schmitt trigger low threshold |
| `counter_up` | 0.7 | Schmitt trigger high threshold |
| `counter_min_frames` | 3 | Minimum frames in state before transition (anti-jitter) |
| `smooth_window` | 5 | Moving average window for signal smoothing |

## Common Adjustments

**Too many false positives (counting non-exercises):**
- Increase `period_strength` (e.g., 0.5 → 0.6)
- Increase `counter_min_frames` (e.g., 3 → 5)

**Missing reps:**
- Decrease `period_strength` (e.g., 0.4 → 0.3)
- Widen `counter_down` / `counter_up` gap

**Merging different exercises into one:**
- Decrease `dtw_threshold` (e.g., 2.0 → 1.5)

**Splitting same exercise into multiple:**
- Increase `dtw_threshold` (e.g., 2.0 → 3.0)

**Slow exercises not detected:**
- Increase `max_period` and `window_frames`

**Fast exercises missed:**
- Decrease `min_period` (e.g., 10 → 5)
```

- [ ] **Step 4: Update README.md**

`README.md`:
```markdown
# exercises-counter

Automatic exercise repetition counter from video. Detects repeating motion
patterns — no predefined exercise list. Patterns are learned and persisted.

## Quick Start

```bash
pip install -e ".[web]"

# Count exercises from video file
python -m demo.cli path/to/video.mp4

# Count from camera
python -m demo.cli --camera 0

# Web dashboard
python -m demo.web.server path/to/video.mp4
# → http://localhost:8000
```

## How It Works

1. **Writer** captures video, runs MediaPipe pose detection, saves landmarks to SQLite
2. **Analyzer** reads landmarks, detects repeating patterns via autocorrelation + DTW
3. New patterns are saved immediately, recognized on next run
4. Events stream to CLI or web dashboard via WebSocket

## Docs

- [Build Instructions](docs/BUILD.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Tuning Guide](docs/TUNING.md)

## Target Platforms

- Raspberry Pi 5 + Camera Module (primary)
- Linux x86_64
- Windows x86_64
- macOS Apple Silicon
- iOS arm64 (C++ core only)
```

- [ ] **Step 5: Commit**

```bash
git add docs/BUILD.md docs/ARCHITECTURE.md docs/TUNING.md README.md
git commit -m "docs: BUILD, ARCHITECTURE, TUNING guides + updated README"
```

---

## Task 17: Update CLAUDE.md

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: Update CLAUDE.md to reflect new architecture**

Replace the exercise-specific sections (squat/pushup/jumping_jack references, hardcoded thresholds) with the auto-detection approach. Key changes:
- Remove predefined exercise list (squat.hpp, pushup.hpp, jumping_jack.hpp)
- Add pattern.hpp, analyzer_core.hpp to file map
- Update algorithm description to autocorrelation + DTW
- Update directory structure
- Mark resolved TODOs

- [ ] **Step 2: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: update CLAUDE.md to match auto-detection architecture"
```

---

## Task 18: GitHub Actions CI

**Files:**
- Create: `.github/workflows/build.yml`

- [ ] **Step 1: Create CI workflow**

`.github/workflows/build.yml`:
```yaml
name: Build & Test

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  cpp-tests:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Configure CMake
        run: cmake -S . -B build -DEXCO_BUILD_TESTS=ON
      - name: Build
        run: cmake --build build
      - name: Test
        run: |
          cd build
          ctest --output-on-failure

  python-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.11"
      - name: Install
        run: pip install -e ".[dev]"
      - name: Run pytest
        run: python -m pytest tests/ -v

  arm64-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: uraimo/run-on-arch-action@v2
        with:
          arch: aarch64
          distro: bookworm
          install: |
            apt-get update && apt-get install -y cmake g++ python3 python3-pip python3-venv
          run: |
            cmake -S . -B build -DEXCO_BUILD_TESTS=ON
            cmake --build build
            cd build && ctest --output-on-failure
```

- [ ] **Step 2: Commit**

```bash
git add .github/workflows/build.yml
git commit -m "ci: multi-platform build + test (x86, arm64, macOS, Windows)"
```

---

## Task 19: Download Reference Video & Smoke Test

- [ ] **Step 1: Install yt-dlp and download reference video**

```bash
pip install yt-dlp
yt-dlp -f "best[height<=720]" -o "tests/fixtures/reference.mp4" "https://www.youtube.com/watch?v=FEMGhHk2_ks"
```

Add to `.gitignore`:
```
tests/fixtures/reference.mp4
```

- [ ] **Step 2: Run CLI on reference video**

```bash
python -m demo.cli tests/fixtures/reference.mp4 --db test_reference.db
```

Watch stdout for exercise events. Press Ctrl+C after video ends.

- [ ] **Step 3: Verify results**

```bash
python -c "
from exco.db import ExcoDB
db = ExcoDB('test_reference.db')
patterns = db.read_patterns()
print(f'Patterns detected: {len(patterns)}')
events = db.read_events_since(0)
for p in patterns:
    count = max((e['count'] for e in events if e['pattern_id'] == p['id']), default=0)
    print(f'  Pattern #{p[\"id\"]}: {count} reps, period={p[\"period_frames\"]} frames')
db.close()
"
```

Expected: multiple patterns detected, each with rep counts > 0.

- [ ] **Step 4: Cleanup test DB**

```bash
rm -f test_reference.db
```

- [ ] **Step 5: Commit .gitignore update**

```bash
git add .gitignore
git commit -m "chore: ignore reference video in fixtures"
```

---

Plan complete and saved to `docs/superpowers/plans/2026-05-05-exercises-counter.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?