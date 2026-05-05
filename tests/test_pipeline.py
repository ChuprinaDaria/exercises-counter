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
