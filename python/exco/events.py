from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class ExerciseEvent:
    pattern_id: int
    count: int
    timestamp: float
