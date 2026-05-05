# Exercises Counter — Design Spec

## Goal

Prototype that counts person's exercises from video (realtime camera or file).
Automatically detects repeating motion patterns — no predefined exercise list.
Patterns are persisted immediately and recognized from first signs on subsequent runs.

Target platform: Raspberry Pi 5 + Camera Module.
Cross-compilation: x86 (Win), Apple Silicon (macOS), arm64 (iOS).

## Architecture

Two independent processes communicating via SQLite:

```
┌─────────────┐         ┌──────────┐         ┌─────────────┐
│   Writer    │──────►  │  SQLite  │  ◄──────│  Analyzer   │
│ video→pose  │         │          │         │ patterns +  │
│ →landmarks  │         │landmarks │         │ counting    │
└─────────────┘         │patterns  │         └─────────────┘
                        │events    │               │
                        └──────────┘               ▼
                             ▲              ┌─────────────┐
                             └──────────────│  Web UI     │
                                            │ (read-only) │
                                            └─────────────┘
```

### Writer Process (Python)

- Captures video frames (camera or file via OpenCV)
- Runs MediaPipe Pose → 33 landmarks per frame
- Writes landmarks to SQLite immediately (every frame)
- No analysis, no buffering — just capture and persist

### Analyzer Process (Python + C++ core)

- Continuously polls SQLite for new landmarks
- Runs signal analysis via C++ core (pybind11 bindings)
- Detects periodicity, extracts patterns, counts reps
- Writes events and new patterns back to SQLite

### Web UI (Python — FastAPI + WebSocket)

- Reads events from SQLite
- Pushes updates to browser via WebSocket
- Simple dashboard: current exercise index, count, timestamp
- Also shows video feed from Writer (optional)

## C++ Core

Pure math, zero dependencies beyond STL. Namespace: `exco`.

### Responsibilities

| Module | Purpose |
|--------|---------|
| `signal.hpp` | Smoothing (moving average), autocorrelation, period detection |
| `pattern.hpp` | Pattern extraction (one cycle), serialization, DTW comparison |
| `counter.hpp` | Schmitt trigger state machine for rep counting |
| `geometry.hpp` | Angle/distance helpers for joint landmarks |

### Pattern Detection Algorithm

1. Sliding window (~3-5 seconds) over landmark time series
2. Compute autocorrelation across all joint signals
3. Find dominant period (if autocorrelation peak > threshold)
4. Extract one cycle as "signature" (normalized time series of key joints)
5. Compare signature against known patterns via DTW (Dynamic Time Warping)
6. Match found → increment count for that pattern, emit event
7. No match → store as new pattern, count = 1

### Pattern Matching Rules

- Different amplitude = different pattern
- Different speed = different pattern (period is part of signature)
- Comparison is strict — only geometrically similar movements match

### Cross-Platform

- C++17 standard (no C++20 — broader compiler support)
- CMake 3.20+ build system
- pybind11 bindings (header-only)
- Compiles via CI for: linux x86, linux arm64, windows, macos, ios

## SQLite Schema

```sql
CREATE TABLE landmarks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp REAL NOT NULL,        -- seconds since start
    frame_id INTEGER NOT NULL,
    joint_id INTEGER NOT NULL,      -- 0-32 (MediaPipe indices)
    x REAL NOT NULL,
    y REAL NOT NULL,
    z REAL NOT NULL,
    visibility REAL NOT NULL
);

CREATE INDEX idx_landmarks_frame ON landmarks(frame_id);
CREATE INDEX idx_landmarks_timestamp ON landmarks(timestamp);

CREATE TABLE patterns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    signature BLOB NOT NULL,        -- serialized pattern (C++ struct → bytes)
    period_frames INTEGER NOT NULL, -- how many frames per one repetition
    dominant_joints TEXT NOT NULL,   -- JSON array of joint indices with most variance
    created_at REAL NOT NULL
);

CREATE TABLE events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    pattern_id INTEGER NOT NULL REFERENCES patterns(id),
    count INTEGER NOT NULL,         -- cumulative count for this pattern in this session
    timestamp REAL NOT NULL
);

CREATE INDEX idx_events_timestamp ON events(timestamp);
```

## Python Layer

### Pose Detection (Protocol-based)

```python
class PoseDetector(Protocol):
    def detect(self, frame: np.ndarray) -> list[Landmark] | None: ...

class MediaPipeBackend:
    """Default implementation. iOS dev can swap for Vision framework."""
    ...
```

### Writer

```python
class LandmarkWriter:
    def __init__(self, source: str | int, db_path: str, detector: PoseDetector): ...
    def run(self) -> None:
        """Capture loop: frame → detect → write to DB. Blocks until source ends."""
```

### Analyzer

```python
class PatternAnalyzer:
    def __init__(self, db_path: str, core: exco.AnalyzerCore): ...
    def run(self) -> None:
        """Poll loop: read landmarks → analyze → write events. Runs forever."""
```

## Demo Application

### CLI

```bash
# Process video file
python -m demo.cli path/to/video.mp4

# Live camera
python -m demo.cli --camera 0

# Both start Writer + Analyzer + print events to stdout
```

### Web

```bash
python -m demo.web.server
# → http://localhost:8000
# Shows: video feed, detected patterns, live counter
```

## Directory Structure

```
exercises-counter/
├── CLAUDE.md
├── README.md
├── pyproject.toml
├── CMakeLists.txt
│
├── cpp/
│   ├── CMakeLists.txt
│   ├── include/exco/
│   │   ├── geometry.hpp
│   │   ├── signal.hpp
│   │   ├── pattern.hpp
│   │   └── counter.hpp
│   ├── src/
│   │   ├── signal.cpp
│   │   ├── pattern.cpp
│   │   └── counter.cpp
│   ├── bindings/python_bindings.cpp
│   └── tests/
│
├── python/exco/
│   ├── __init__.py
│   ├── pose/
│   │   ├── base.py          # Protocol
│   │   └── mediapipe_backend.py
│   ├── db.py                # SQLite schema + read/write
│   ├── writer.py
│   ├── analyzer.py
│   └── events.py
│
├── demo/
│   ├── cli.py
│   └── web/
│       ├── server.py
│       └── static/index.html
│
├── tests/
├── docs/
│   ├── BUILD.md
│   ├── ARCHITECTURE.md
│   └── TUNING.md
│
└── .github/workflows/build.yml
```

## Success Criteria

1. Process reference video (morning gymnastics) — correctly count each distinct repeating exercise
2. Run on Raspberry Pi 5 at ≥15 FPS (pose detection is the bottleneck, not our core)
3. Patterns persist between sessions — recognized immediately on restart
4. Web UI shows live count updates via WebSocket
5. C++ core compiles on all target platforms via CI

## Out of Scope (for this prototype)

- Multi-person detection (single person only)
- Exercise naming (patterns get numeric IDs, no semantic labels)
- Cloud sync of patterns
- Mobile native apps (iOS/Android) — only the C++ core is portable
- Audio/voice feedback

## Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| DB | SQLite | No server, fast on RPi, single file |
| Pattern comparison | DTW | Handles slight timing variations within same pattern |
| Strictness | Amplitude+speed matter | User requirement: different depth/speed = different pattern |
| C++ standard | C++17 | Broad compiler support |
| Pose backend | MediaPipe (Python) | C++ MediaPipe build is pain, not worth it |
| Process communication | SQLite polling | Simple, reliable, zero infrastructure |
| Analyzer polling interval | ~100ms | Fast enough for real-time feel, low CPU |
