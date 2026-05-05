from __future__ import annotations

import argparse
import asyncio
import json
import multiprocessing
import time
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

from exco.db import ExcoDB
from exco.writer import LandmarkWriter
from exco.analyzer import PatternAnalyzer
from exco.pose.mediapipe_backend import MediaPipeBackend

STATIC_DIR = Path(__file__).parent / "static"

app = FastAPI(title="Exercise Counter")
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

DB_PATH = "exercises.db"


@app.get("/")
async def index() -> HTMLResponse:
    html = (STATIC_DIR / "index.html").read_text()
    return HTMLResponse(html)


@app.websocket("/ws/events")
async def ws_events(ws: WebSocket) -> None:
    await ws.accept()
    db = ExcoDB(DB_PATH)
    last_id = 0
    try:
        while True:
            events = db.read_events_since(last_id)
            for e in events:
                await ws.send_text(json.dumps({
                    "pattern_id": e["pattern_id"],
                    "count": e["count"],
                    "timestamp": round(e["timestamp"], 2),
                }))
                last_id = e["id"]
            await asyncio.sleep(0.2)
    except WebSocketDisconnect:
        pass
    finally:
        db.close()


def run_writer(source: str | int, db_path: str) -> None:
    detector = MediaPipeBackend(model_complexity=1)
    writer = LandmarkWriter(source, db_path, detector)
    writer.run()


def run_analyzer(db_path: str) -> None:
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
