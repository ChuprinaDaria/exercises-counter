import tempfile
import os
import pytest
from exco.db import ExcoDB


def test_create_tables():
    with tempfile.TemporaryDirectory() as d:
        db = ExcoDB(os.path.join(d, "test.db"))
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
