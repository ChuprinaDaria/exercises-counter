# Multi-Device Camera Support

## Problem

Currently exercises-counter runs on a single machine: camera, pose detection, analysis, and dashboard all on localhost. Users want to:

1. Use cameras on different devices (phones, tablets, Raspberry Pi, laptops)
2. View dashboard from any device on the network
3. Auto-discover available cameras without manual IP configuration

## Architecture

```
Device A (phone/RPi/laptop)          Server (any machine)
┌─────────────────────┐      ┌──────────────────────────────┐
│ Camera               │      │ FastAPI server                │
│ MediaPipe (optional) │─────>│  ├── /api/devices   (discovery)│
│ Writer agent         │ HTTP │  ├── /ws/landmarks  (stream)  │
└─────────────────────┘      │  ├── /ws/events     (stream)  │
                              │  ├── /api/stats     (query)   │
Device B (another camera)     │  ├── Analyzer (C++ core)      │
┌─────────────────────┐      │  └── SQLite DB                │
│ Camera               │─────>│                                │
│ Writer agent         │ HTTP └──────────────────────────────┘
└─────────────────────┘              │
                              Browser (any device)
                              ┌──────────────────┐
                              │ Dashboard          │
                              │ Stick figure(s)    │
                              │ Multi-cam selector │
                              └──────────────────┘
```

### Two modes of operation

**Mode A: Landmarks over network (lightweight devices)**
Device runs only camera + MediaPipe, sends landmarks to server via HTTP/WebSocket.
Server runs analyzer. Good for Raspberry Pi, phones.

**Mode B: Full local processing**
Device runs camera + MediaPipe + Writer + Analyzer locally, syncs events to central server.
Good for powerful laptops. Works offline, syncs when connected.

## Discovery Protocol

### mDNS/Zeroconf (LAN)

Server advertises `_exco._tcp.local` service via mDNS.
Devices discover server automatically on the same network.

```python
# Server side
from zeroconf import Zeroconf, ServiceInfo
info = ServiceInfo(
    "_exco._tcp.local.",
    "ExCo Server._exco._tcp.local.",
    addresses=[socket.inet_aton(local_ip)],
    port=8000,
    properties={"version": "0.1.0"}
)
zeroconf.register_service(info)

# Device side
from zeroconf import ServiceBrowser
browser = ServiceBrowser(zeroconf, "_exco._tcp.local.", handler)
```

### Device registration

Devices POST to server on connect:

```
POST /api/devices/register
{
    "device_id": "rpi-kitchen-01",
    "device_name": "Kitchen RPi",
    "camera_info": {"resolution": [640, 480], "fps": 30},
    "capabilities": ["camera", "mediapipe"],
    "mode": "landmarks"  // or "full"
}
```

Server responds with device config:

```
{
    "device_id": "rpi-kitchen-01",
    "stream_url": "ws://server:8000/ws/ingest/{device_id}",
    "analyzer_config": { ... current AnalyzerConfig ... }
}
```

## New API Endpoints

### Server

```
GET  /api/devices              — list connected devices
POST /api/devices/register     — device registration
WS   /ws/ingest/{device_id}    — receive landmarks from device
GET  /api/devices/{id}/stats   — per-device statistics
WS   /ws/landmarks/{device_id} — stream landmarks for specific device to dashboard
```

### Dashboard changes

- Device selector dropdown (or multi-view)
- Per-device stick figure panels
- Aggregated stats across devices
- Device status indicators (online/offline/latency)

## Device Agent (runs on camera device)

Lightweight Python script (~100 lines):

```
exco-agent --server auto    # mDNS discovery
exco-agent --server 192.168.1.100:8000  # manual
exco-agent --camera 0       # camera index
exco-agent --mode landmarks  # send landmarks only (default)
exco-agent --mode full       # local processing + sync events
```

### Package: `exco-agent`

Minimal dependencies for Mode A:
- mediapipe
- opencv-python
- websockets (or httpx)

No C++ core needed. Device just sends landmarks.

### Data format (landmarks over WebSocket)

```json
{
    "device_id": "rpi-kitchen-01",
    "frame_id": 1234,
    "timestamp": 45.67,
    "landmarks": [
        {"x": 0.5, "y": 0.3, "z": 0.0, "v": 0.95},
        ...  // 33 landmarks
    ]
}
```

Binary alternative (for constrained devices): MessagePack.
33 landmarks * 4 floats * 4 bytes = 528 bytes per frame.
At 30fps = ~16 KB/s per device. Negligible.

## Platform-specific Camera Backends

### Raspberry Pi (Camera Module v2/v3)

```python
# PiCameraBackend implements PoseDetector Protocol
import picamera2
class PiCameraBackend:
    def __init__(self):
        self.cam = Picamera2()
        self.cam.configure(self.cam.create_preview_configuration(
            main={"size": (640, 480), "format": "RGB888"}
        ))
```

### iOS (Vision framework)

Swift app implements camera capture + Apple Vision pose detection.
Sends landmarks to server in same JSON format.
No MediaPipe dependency. Uses ARKit/Vision `VNDetectHumanBodyPoseRequest`.

Protocol PoseDetector already designed for this swap — `detect(frame) -> [Landmark]?`.

### Android (ML Kit Pose)

Similar to iOS. Uses Google ML Kit Pose Detection.
Kotlin/Java app sends landmarks via WebSocket.

### Web browser (MediaPipe JS)

Camera via `getUserMedia()`, pose detection via `@mediapipe/tasks-vision` in browser.
Sends landmarks to server via existing WebSocket.
Zero install for end users.

## DB Schema Changes

```sql
-- Add device tracking to landmarks
ALTER TABLE landmarks ADD COLUMN device_id TEXT DEFAULT 'local';

-- Device registry
CREATE TABLE devices (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    last_seen REAL NOT NULL,
    capabilities TEXT NOT NULL,  -- JSON
    mode TEXT NOT NULL DEFAULT 'landmarks'
);

CREATE INDEX idx_landmarks_device ON landmarks(device_id, frame_id);
```

## Implementation Order

### Phase 1: Network-accessible dashboard
- [ ] Server binds to 0.0.0.0
- [ ] Lazy MediaPipe import (done)
- [ ] Dashboard works from any browser on LAN

### Phase 2: Landmark ingest endpoint
- [ ] `POST /api/devices/register`
- [ ] `WS /ws/ingest/{device_id}`
- [ ] DB schema: device_id column
- [ ] Server-side analyzer per device

### Phase 3: Device agent
- [ ] `exco-agent` CLI package
- [ ] mDNS discovery (zeroconf)
- [ ] Camera backends: OpenCV (default), PiCamera2 (RPi)
- [ ] Auto-reconnect on disconnect

### Phase 4: Multi-device dashboard
- [ ] Device selector/multi-view
- [ ] Per-device stick figures
- [ ] Aggregated statistics
- [ ] Device health monitoring

### Phase 5: Platform-specific agents
- [ ] iOS app (Vision framework)
- [ ] Android app (ML Kit)
- [ ] Browser-based agent (MediaPipe JS)
- [ ] Mode B: full local processing + event sync

## Constraints

- SQLite remains the single source of truth (server-side)
- C++ core stays dependency-free (STL only)
- No authentication in v1 (LAN-only, trusted network)
- Max 4 simultaneous camera feeds in v1
- Device agent must work on Python 3.11+ with minimal deps
- Bandwidth: landmarks only, never raw video over network
