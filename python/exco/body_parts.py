from __future__ import annotations

# Canonical mapping: MediaPipe joint index → body part name.
# Only major joints (11-28); face/hands/feet are excluded from analysis.
JOINT_TO_PART: dict[int, str] = {
    11: "Shoulders",
    12: "Shoulders",
    13: "Arms",
    14: "Arms",
    15: "Arms",
    16: "Arms",
    23: "Hips",
    24: "Hips",
    25: "Legs",
    26: "Legs",
    27: "Legs",
    28: "Legs",
}


def joints_to_body_parts(joint_ids: list[int]) -> list[str]:
    """Return sorted unique body part names for given joint indices."""
    parts = {JOINT_TO_PART[j] for j in joint_ids if j in JOINT_TO_PART}
    return sorted(parts)


def exercise_name(joint_ids: list[int]) -> str:
    """Human-readable exercise name from dominant joints.

    Examples: "Arms", "Legs + Hips", "Full Body".
    """
    parts = joints_to_body_parts(joint_ids)
    if not parts:
        return "Unknown"
    if len(parts) >= 4:
        return "Full Body"
    return " + ".join(parts)
