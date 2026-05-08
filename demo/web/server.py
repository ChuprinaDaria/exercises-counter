from __future__ import annotations

import argparse
import asyncio
import json
import multiprocessing
import time
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

from exco.db import ExcoDB
from exco.body_parts import joints_to_body_parts
from exco.routine import RoutineDetector

STATIC_DIR = Path(__file__).parent / "static"

app = FastAPI(title="Exercise Counter")
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

DB_PATH = "exercises.db"
_routine_detector = RoutineDetector()


@app.get("/")
async def index() -> HTMLResponse:
    html = (STATIC_DIR / "index.html").read_text(encoding="utf-8")
    return HTMLResponse(html)


@app.get("/api/stats")
async def api_stats() -> dict:
    db = ExcoDB(DB_PATH)
    patterns = db.read_patterns()
    events = db.read_events_since(0)
    db.close()

    # Rebuild routine state from existing events
    routine_det = RoutineDetector()
    last_pattern = None
    for e in events:
        pid = e["pattern_id"]
        if e["count"] == 0 and pid != last_pattern:
            routine_det.push(pid)
            last_pattern = pid

    routine = routine_det.routine
    # Update global detector to match
    global _routine_detector
    _routine_detector = routine_det

    return {
        "patterns": [
            {
                "id": p["id"],
                "dominant_joints": json.loads(p["dominant_joints"]),
                "body_parts": joints_to_body_parts(json.loads(p["dominant_joints"])),
            }
            for p in patterns
        ],
        "events": [
            {"pattern_id": e["pattern_id"], "count": e["count"],
             "timestamp": round(e["timestamp"], 2)}
            for e in events
        ],
        "routine": {
            "routine_id": routine.routine_id,
            "sequence": routine.sequence,
            "sets": routine.sets,
        } if routine else None,
    }


@app.websocket("/ws/events")
async def ws_events(ws: WebSocket) -> None:
    await ws.accept()
    db = ExcoDB(DB_PATH)
    last_id = db.max_event_id()
    last_pattern = None
    try:
        while True:
            events = db.read_events_since(last_id)
            for e in events:
                await ws.send_text(json.dumps({
                    "type": "exercise",
                    "pattern_id": e["pattern_id"],
                    "count": e["count"],
                    "timestamp": round(e["timestamp"], 2),
                }))
                last_id = e["id"]

                # Feed pattern transitions to routine detector
                pid = e["pattern_id"]
                if e["count"] == 0 and pid != last_pattern:
                    routine_event = _routine_detector.push(pid)
                    last_pattern = pid
                    if routine_event:
                        await ws.send_text(json.dumps({
                            "type": "routine",
                            "routine_id": routine_event.routine_id,
                            "sequence": routine_event.sequence,
                            "sets": routine_event.sets,
                        }))
            await asyncio.sleep(0.2)
    except WebSocketDisconnect:
        pass
    finally:
        db.close()


@app.websocket("/ws/landmarks")
async def ws_landmarks(ws: WebSocket) -> None:
    await ws.accept()
    db = ExcoDB(DB_PATH)
    # Start from latest frame (don't replay old data on reconnect)
    last_frame = db.max_frame_id()
    try:
        while True:
            rows = db.read_landmarks_since(last_frame + 1, 0.0)
            if not rows:
                await asyncio.sleep(0.05)
                continue

            frames: dict[int, list[dict]] = {}
            for r in rows:
                frames.setdefault(r["frame_id"], []).append(r)

            for fid in sorted(frames.keys()):
                joints = sorted(frames[fid], key=lambda r: r["joint_id"])
                if len(joints) != 33:
                    last_frame = fid
                    continue
                payload = [
                    {"x": j["x"], "y": j["y"], "v": j["visibility"]}
                    for j in joints
                ]
                await ws.send_text(json.dumps({"frame": fid, "landmarks": payload}))
                last_frame = fid
                await asyncio.sleep(0.033)  # ~30fps playback
    except WebSocketDisconnect:
        pass
    finally:
        db.close()


def run_writer(source: str | int, db_path: str) -> None:
    from exco.pose.mediapipe_backend import MediaPipeBackend
    from exco.writer import LandmarkWriter
    detector = MediaPipeBackend(model_complexity=1)
    writer = LandmarkWriter(source, db_path, detector)
    writer.run()


def run_analyzer(db_path: str) -> None:
    from exco.analyzer import PatternAnalyzer
    analyzer = PatternAnalyzer(db_path)
    analyzer.run()


def main() -> None:
    global DB_PATH
    parser = argparse.ArgumentParser(description="Exercise counter web demo")
    parser.add_argument("video", nargs="?", help="Path to video file")
    parser.add_argument("--camera", type=int, help="Camera index")
    parser.add_argument("--db", default="exercises.db")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    DB_PATH = args.db

    procs: list[multiprocessing.Process] = []
    if args.video is None and args.camera is None:
        from exco.writer import find_camera
        cam = find_camera()
        if cam is not None:
            print(f"Auto-detected camera at index {cam}")
            args.camera = cam
        else:
            print("No camera found. Starting server without capture "
                  "(provide a video file or --camera N to enable)")

    if args.video or args.camera is not None:
        source: str | int = args.video if args.video else args.camera
        wp = multiprocessing.Process(target=run_writer, args=(source, args.db))
        ap = multiprocessing.Process(target=run_analyzer, args=(args.db,))
        wp.start()
        time.sleep(0.5)
        ap.start()
        procs = [wp, ap]

    try:
        uvicorn.run(app, host=args.host, port=args.port)
    finally:
        for p in procs:
            p.terminate()
            p.join(timeout=3)


if __name__ == "__main__":
    main()
