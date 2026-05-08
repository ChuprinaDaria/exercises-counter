# exercises-counter

Automatic exercise repetition counter from video. Detects repeating motion
patterns — no predefined exercise list. Patterns are learned and persisted.

## Quick Start

```bash
pip install -e ".[web]"

# Auto-detect camera and start counting
python -m demo.cli

# Or specify a video file
python -m demo.cli path/to/video.mp4

# Or specify camera index explicitly
python -m demo.cli --camera 0

# Web dashboard (auto-detects camera if no source given)
python -m demo.web.server
# → http://localhost:8000

# Web dashboard with video file
python -m demo.web.server path/to/video.mp4
```

## How It Works

### Exercise Detection
Individual exercises detected automatically — no predefined list needed.

1. **Writer** captures video → MediaPipe pose detection → landmarks to SQLite
2. **Analyzer** reads landmarks → autocorrelation finds periodicity → DTW matches patterns
3. Schmitt trigger counts each repetition (up-down cycle)
4. New exercises are saved immediately, recognized on next run

### Routine Detection
When exercises appear in a repeating sequence, that's a **routine**.

Example: arm raises → squats → bends → arm raises → squats → bends = routine [#1→#2→#3], 2 sets.

Routine is detected after ≥2 complete passes through the same sequence. Once detected, the routine is locked — further passes increment the set counter.

### Terminology

| Term | Meaning | Example |
|------|---------|---------|
| **Exercise** | One type of detected movement | Arm raises, squats |
| **Count** | How many repetitions within an exercise | 12 arm raises = count 12 |
| **Routine** | Ordered sequence of different exercises that repeats | [arm raises → squats → bends] |
| **Set** | One full pass through a routine | Did all 3 exercises once = 1 set |

Events stream to CLI or web dashboard via WebSocket.

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
