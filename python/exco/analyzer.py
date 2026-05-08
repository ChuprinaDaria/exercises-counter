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
                            sig_bytes = bytes(cpp.serialize_pattern(
                                self._make_pattern(event)
                            ))
                            joints_json = json.dumps(list(event.dominant_joints))
                            new_id = self._db.write_pattern(
                                sig_bytes, event.period_frames, joints_json
                            )
                            # Signal new pattern as event (count=0)
                            self._db.write_event(new_id, 0, ts)
                        elif event.pattern_started:
                            # Known pattern resumed — write event with count=0
                            self._db.write_event(
                                event.pattern_id, 0, ts
                            )
                        else:
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
