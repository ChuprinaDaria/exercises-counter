from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

import numpy as np
from numpy.typing import NDArray


@dataclass(frozen=True)
class Landmark:
    x: float
    y: float
    z: float
    visibility: float


class PoseDetector(Protocol):
    def detect(self, frame: NDArray[np.uint8]) -> list[Landmark] | None:
        """Return 33 landmarks or None if no person detected."""
        ...

    def close(self) -> None: ...
