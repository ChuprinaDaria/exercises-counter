from __future__ import annotations

import urllib.request
from pathlib import Path

import mediapipe as mp
import numpy as np
from numpy.typing import NDArray

from exco.pose.base import Landmark

_mp_tasks = mp.tasks
_vision = _mp_tasks.vision
_RunningMode = _vision.RunningMode

_DEFAULT_MODEL_URL = (
    "https://storage.googleapis.com/mediapipe-models/"
    "pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task"
)
_DEFAULT_MODEL_PATH = Path(__file__).parent / "pose_landmarker_lite.task"


def _ensure_model(model_path: Path) -> None:
    if not model_path.exists():
        print(f"[MediaPipeBackend] Downloading pose model to {model_path} ...")
        urllib.request.urlretrieve(_DEFAULT_MODEL_URL, model_path)
        print("[MediaPipeBackend] Download complete.")


class MediaPipeBackend:
    def __init__(
        self,
        model_complexity: int = 1,
        min_detection_confidence: float = 0.5,
        min_tracking_confidence: float = 0.5,
        model_path: str | None = None,
    ) -> None:
        resolved = Path(model_path) if model_path else _DEFAULT_MODEL_PATH
        _ensure_model(resolved)

        options = _vision.PoseLandmarkerOptions(
            base_options=_mp_tasks.BaseOptions(model_asset_path=str(resolved)),
            running_mode=_RunningMode.IMAGE,
            num_poses=1,
            min_pose_detection_confidence=min_detection_confidence,
            min_pose_presence_confidence=min_detection_confidence,
            min_tracking_confidence=min_tracking_confidence,
        )
        self._landmarker = _vision.PoseLandmarker.create_from_options(options)

    def detect(self, frame: NDArray[np.uint8]) -> list[Landmark] | None:
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=frame)
        result = self._landmarker.detect(mp_image)
        if not result.pose_landmarks:
            return None
        return [
            Landmark(x=lm.x, y=lm.y, z=lm.z, visibility=lm.presence)
            for lm in result.pose_landmarks[0]
        ]

    def close(self) -> None:
        self._landmarker.close()
