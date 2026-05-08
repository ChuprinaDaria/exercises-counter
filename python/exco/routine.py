from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class RoutineEvent:
    routine_id: int
    sequence: list[int]
    sets: int


class RoutineDetector:
    """Detect repeating sequences of pattern transitions.

    Feed pattern_id each time the active pattern changes.
    Once a shortest repeating prefix is found (≥2 sets), the routine
    is locked — further sets increment the counter but the sequence
    does not change.
    """

    def __init__(self) -> None:
        self._transitions: list[int] = []
        self._routine_sequence: list[int] | None = None
        self._routine_id: int = 1
        self._sets: int = 0

    @property
    def routine(self) -> RoutineEvent | None:
        if self._routine_sequence is None:
            return None
        return RoutineEvent(
            routine_id=self._routine_id,
            sequence=list(self._routine_sequence),
            sets=self._sets,
        )

    def push(self, pattern_id: int) -> RoutineEvent | None:
        """Register a pattern transition. Returns RoutineEvent if sets changed."""
        # Collapse consecutive duplicates
        if self._transitions and self._transitions[-1] == pattern_id:
            return None
        self._transitions.append(pattern_id)

        # Routine already locked — check if another set completed
        if self._routine_sequence is not None:
            return self._check_next_set()

        # Try to find a repeating prefix
        return self._try_detect()

    def _try_detect(self) -> RoutineEvent | None:
        seq = self._transitions
        n = len(seq)
        # Try prefix lengths from 2 up to half the sequence
        for prefix_len in range(2, n // 2 + 1):
            prefix = seq[:prefix_len]
            sets = 0
            for i in range(0, n, prefix_len):
                chunk = seq[i : i + prefix_len]
                if chunk == prefix:
                    sets += 1
                else:
                    break
            if sets >= 2:
                self._routine_sequence = prefix
                self._sets = sets
                return self.routine
        return None

    def _check_next_set(self) -> RoutineEvent | None:
        assert self._routine_sequence is not None
        seq_len = len(self._routine_sequence)
        expected_sets = len(self._transitions) // seq_len
        # Verify all complete chunks match
        for s in range(expected_sets):
            chunk = self._transitions[s * seq_len : (s + 1) * seq_len]
            if chunk != self._routine_sequence:
                return None
        if expected_sets > self._sets:
            self._sets = expected_sets
            return self.routine
        return None
