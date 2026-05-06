from __future__ import annotations

import argparse
import multiprocessing
import sys
import time

from exco.db import ExcoDB
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
    db = ExcoDB(db_path)
    last_id = 0
    try:
        while True:
            events = db.read_events_since(last_id)
            for e in events:
                print(
                    f"[{e['timestamp']:.2f}s] "
                    f"exercise #{e['pattern_id']}  "
                    f"count: {e['count']}"
                )
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
