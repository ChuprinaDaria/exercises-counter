# exercises-counter

Automatic exercise repetition counter from video/camera. Detects repeating motion patterns without a predefined exercise list. Patterns are learned on the fly and persist between sessions.

## What It Does

You point a camera at a person exercising. The system:

1. Detects body pose in real time (shoulders, elbows, wrists, hips, knees, ankles)
2. Finds repeating movements automatically (no need to tell it "this is a squat")
3. Counts repetitions for each detected exercise
4. Shows results on a live web dashboard with a stick figure, rep counters, and timeline

Each unique movement becomes a **pattern**. Patterns are saved to a database — next time you do the same exercise, it's recognized immediately.

## How Counting Works

### Step by Step

```
Camera frame
    ↓
MediaPipe detects 33 body landmarks (x, y, z + visibility)
    ↓
System tracks y-coordinate of each major joint over a 2-second sliding window
    ↓
Finds the 5 joints that move the most (dominant joints)
    ↓
Builds a composite signal by averaging those joints, then smooths it
    ↓
Autocorrelation finds the repetition period (how many frames per cycle)
    ↓
Extracts one cycle, normalizes to 0..1
    ↓
Compares against known patterns using DTW (Dynamic Time Warping)
    ↓
Match found? → Count rep with Schmitt trigger
No match?   → After 0.5 sec delay, save as new pattern
```

### What Makes Exercises Different

Two movements are considered **different exercises** if:

- **Different body parts** move (arms vs legs) — checked via dominant joint overlap (need ≥60% match)
- **Different shape** of movement — checked via DTW distance on the normalized cycle
- **Different speed** — period is part of the pattern signature

So arm raises and squats will always be separate patterns, even if they look vaguely similar in signal shape.

### What is a Pattern

A pattern captures:
- **Signature** — the normalized shape of one repetition cycle (array of floats, 0..1)
- **Period** — how many frames one rep takes
- **Dominant joints** — which body parts are involved (fixed at creation, never changes)

The signature evolves slightly over time (80% old + 20% new on each match) to adapt to natural variation. But the dominant joints are locked — this prevents different exercises from merging into one pattern.

### Rep Counting (Schmitt Trigger)

The composite signal oscillates between 0 and 1 for each rep. A Schmitt trigger with two thresholds prevents double-counting:

- Signal rises above 0.7 → armed
- Signal falls below 0.3 → count +1
- Minimum 3 frames between triggers (anti-jitter)

### Routine Detection

When exercises appear in a repeating sequence, that's a **routine**.

Example: arm raises → squats → bends → arm raises → squats → bends = routine [#1 → #2 → #3], 2 sets.

A routine is detected after ≥2 complete passes through the same sequence of exercises.

## Architecture

Two independent processes communicate via SQLite (WAL mode):

```
┌──────────────┐         ┌──────────────┐
│    Writer     │         │   Analyzer   │
│              │         │              │
│ Camera/Video  │         │ C++ core:    │
│ → MediaPipe   │  SQLite  │ signal math  │
│ → landmarks   │────────→│ DTW matching │
│ → DB write    │         │ rep counting │
│              │         │ → events DB  │
└──────────────┘         └──────────────┘
                              │
                              ↓
                    ┌──────────────────┐
                    │   Web Dashboard  │
                    │                  │
                    │ FastAPI + WS     │
                    │ Stick figure     │
                    │ Exercise cards   │
                    │ Timeline chart   │
                    └──────────────────┘
```

**Why this split:**
- MediaPipe (pose detection) is the bottleneck — runs in its own process
- C++ core is pure math (geometry, signal processing, DTW) — no dependencies besides STL
- SQLite WAL allows concurrent read/write from separate processes
- The C++ core can be ported to iOS/Android independently — just swap the pose backend

### Protocol-Based Design

`PoseDetector` is a Python Protocol. MediaPipe is one implementation. On iOS, you'd swap it for Apple Vision framework. The C++ core doesn't care where landmarks come from.

## Quick Start

```bash
# Install (builds C++ extension automatically)
pip install -e ".[web]"

# Web dashboard — auto-detects camera
python -m demo.web.server
# → http://localhost:8000

# Web dashboard with specific camera
python -m demo.web.server --camera 0

# Web dashboard with video file
python -m demo.web.server path/to/video.mp4

# CLI mode
python -m demo.cli --camera 0
python -m demo.cli path/to/video.mp4
```

### Build Requirements

- Python 3.11+
- C++17 compiler (MSVC on Windows, GCC/Clang on Linux/macOS)
- CMake 3.20+

### What You See on the Dashboard

- **Stick figure** — real-time body skeleton (arms red, legs green, torso blue)
- **Exercise cards** — one per detected pattern, shows rep count and body parts involved
- **All Reps** — total repetitions across all exercises
- **Duration** — session length
- **Timeline** — chart of reps over time
- **Event log** — live feed of pattern detections and rep counts

## Terminology

| Term | Meaning | Example |
|------|---------|---------|
| **Pattern** | One type of detected movement | Arm raises, squats |
| **Rep** | One repetition within a pattern | One arm raise up-and-down |
| **Count** | Total reps for a pattern | 12 arm raises = count 12 |
| **Routine** | Ordered sequence of patterns that repeats | [arm raises → squats → bends] |
| **Set** | One full pass through a routine | Did all 3 exercises once = 1 set |

## Configuration

Key parameters in `AnalyzerConfig` (see [Tuning Guide](docs/TUNING.md)):

| Parameter | Default | What It Does |
|-----------|---------|-------------|
| `window_frames` | 60 | Sliding window size (~2 sec at 30fps) |
| `min_period` / `max_period` | 10 / 60 | Allowed rep cycle range in frames |
| `period_strength` | 0.3 | Autocorrelation threshold — lower = more sensitive |
| `dtw_threshold` | 0.8 | Max DTW distance for pattern match — lower = stricter |
| `counter_up` / `counter_down` | 0.7 / 0.3 | Schmitt trigger thresholds |
| `smooth_window` | 5 | Signal smoothing window |

## Project Structure

```
exercises-counter/
├── cpp/                    # C++ core (pure math, no dependencies)
│   ├── include/exco/       # Headers: geometry, signal, pattern, counter
│   ├── src/                # Implementation
│   ├── bindings/           # pybind11 Python bindings
│   └── tests/              # doctest unit tests
├── python/exco/            # Python layer
│   ├── pose/               # Pose detection (MediaPipe backend)
│   ├── db.py               # SQLite read/write
│   ├── writer.py           # Video → pose → DB
│   ├── analyzer.py         # DB → C++ core → events
│   └── routine.py          # Routine detection
├── demo/
│   ├── cli.py              # CLI demo
│   └── web/                # FastAPI web dashboard
└── docs/                   # Architecture, tuning, specs
```

## Docs

- [Build Instructions](docs/BUILD.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Tuning Guide](docs/TUNING.md)
- [Routine Detection Spec](docs/ROUTINE_SPEC.md)

## Target Platforms

- Raspberry Pi 5 + Camera Module (primary)
- Linux x86_64
- Windows x86_64
- macOS Apple Silicon
- iOS arm64 (C++ core only)
