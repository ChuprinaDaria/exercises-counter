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
