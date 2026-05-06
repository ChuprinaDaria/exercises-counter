from __future__ import annotations

import time
import cv2

from exco.db import ExcoDB
from exco.pose.base import PoseDetector


def find_camera(max_index: int = 4) -> int | None:
    """Try camera indices 0..max_index-1, return first that opens."""
    for i in range(max_index):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            cap.release()
            return i
    return None


class LandmarkWriter:
    def __init__(
        self,
        source: str | int,
        db_path: str,
        detector: PoseDetector,
    ) -> None:
        self._source = source
        self._db = ExcoDB(db_path)
        self._detector = detector

    def run(self) -> None:
        cap = cv2.VideoCapture(self._source)
        if not cap.isOpened():
            raise RuntimeError(f"Cannot open video source: {self._source}")

        frame_id = 0
        start_time = time.monotonic()

        try:
            while True:
                ret, frame = cap.read()
                if not ret:
                    break

                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                landmarks = self._detector.detect(rgb)

                if landmarks is not None:
                    ts = time.monotonic() - start_time
                    rows = [
                        (ts, frame_id, j, lm.x, lm.y, lm.z, lm.visibility)
                        for j, lm in enumerate(landmarks)
                    ]
                    self._db.write_landmarks(rows)

                frame_id += 1
        finally:
            cap.release()
            self._detector.close()
            self._db.close()
