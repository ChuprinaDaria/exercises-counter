from __future__ import annotations

import argparse
import multiprocessing
import sys
import time

from exco.db import ExcoDB
from exco.body_parts import exercise_name
from exco.writer import LandmarkWriter, find_camera
from exco.analyzer import PatternAnalyzer
from exco.pose.mediapipe_backend import MediaPipeBackend


def run_writer(source: str | int, db_path: str) -> None:
    detector = MediaPipeBackend(model_complexity=1)
    writer = LandmarkWriter(source, db_path, detector)
    writer.run()


def run_analyzer(db_path: str) -> None:
    analyzer = PatternAnalyzer(db_path)
    analyzer.run()


def run_monitor(db_path: str) -> None:
    """Print events to stdout as they appear."""
    import json
    db = ExcoDB(db_path)
    last_id = 0
    pattern_names: dict[int, str] = {}
    try:
        while True:
            # Load names for any new patterns (with disambiguation)
            all_patterns = db.read_patterns()
            new_ids = [p["id"] for p in all_patterns if p["id"] not in pattern_names]
            if new_ids:
                name_count: dict[str, int] = {}
                ordered: list[tuple[int, str]] = []
                for p in all_patterns:
                    joints = json.loads(p["dominant_joints"])
                    base = exercise_name(joints)
                    name_count[base] = name_count.get(base, 0) + 1
                    ordered.append((p["id"], base))
                seen: dict[str, int] = {}
                for pid, base in ordered:
                    if name_count[base] > 1:
                        seen[base] = seen.get(base, 0) + 1
                        pattern_names[pid] = f"{base} #{seen[base]}"
                    else:
                        pattern_names[pid] = base

            events = db.read_events_since(last_id)
            for e in events:
                pid = e["pattern_id"]
                name = pattern_names.get(pid, f"#{pid}")
                if e["count"] == 0:
                    print(f"[{e['timestamp']:.2f}s] {name} started")
                else:
                    print(f"[{e['timestamp']:.2f}s] {name} — rep {e['count']}")
                last_id = e["id"]
            time.sleep(0.2)
    except KeyboardInterrupt:
        pass
    finally:
        db.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Exercise counter CLI")
    parser.add_argument("video", nargs="?", help="Path to video file")
    parser.add_argument("--camera", type=int, help="Camera index (e.g. 0)")
    parser.add_argument(
        "--db", default="exercises.db", help="SQLite database path"
    )
    args = parser.parse_args()

    if args.video is None and args.camera is None:
        cam = find_camera()
        if cam is None:
            parser.error("No camera found. Provide a video file or --camera N")
        print(f"Auto-detected camera at index {cam}")
        args.camera = cam

    source: str | int = args.video if args.video else args.camera

    writer_proc = multiprocessing.Process(
        target=run_writer, args=(source, args.db)
    )
    analyzer_proc = multiprocessing.Process(
        target=run_analyzer, args=(args.db,)
    )

    writer_proc.start()
    time.sleep(0.5)  # let writer create DB first
    analyzer_proc.start()

    try:
        run_monitor(args.db)
    except KeyboardInterrupt:
        pass
    finally:
        writer_proc.terminate()
        analyzer_proc.terminate()
        writer_proc.join(timeout=3)
        analyzer_proc.join(timeout=3)


if __name__ == "__main__":
    main()
