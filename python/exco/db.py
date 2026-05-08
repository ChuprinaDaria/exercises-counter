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

    def max_frame_id(self) -> int:
        cur = self._conn.execute("SELECT MAX(frame_id) FROM landmarks")
        row = cur.fetchone()
        return row[0] if row[0] is not None else -1

    def close(self) -> None:
        if hasattr(self._local, "conn"):
            self._local.conn.close()
