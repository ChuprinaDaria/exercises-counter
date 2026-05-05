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

Captures video frames, runs MediaPipe Pose, writes 33 landmarks per frame to SQLite. No analysis — just capture and persist.

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
