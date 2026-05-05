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
| `counter_min_frames` | 3 | Min frames in state before transition (anti-jitter) |
| `smooth_window` | 5 | Moving average window |

## Common Adjustments

**Too many false positives:**
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
