CLAUDE.md
Системний промпт для роботи з проєктом exercises-counter. Читай це до будь-якої дії в репо.


Що це за проєкт
Прототип системи автоматичного підрахунку повторень фізичних вправ за відео. Джерело — realtime video stream (камера) або відеофайл. Система автоматично знаходить повторювані патерни руху без попередньо визначеного списку вправ.

Вихід — потік подій:
{pattern_id: int, count: int, timestamp: float, pattern_started: bool}

Патерни зберігаються одразу в БД і розпізнаються з перших рухів при наступному запуску.
При відновленні відомого патерну генерується подія pattern_started (count=0).

Цільова платформа №1 — Raspberry Pi 5 + Camera Module.
Цільові платформи №2 — x86 Windows, Apple Silicon macOS, arm64 iOS.


Архітектурне рішення
Два незалежні процеси + SQLite як IPC:

Writer (Python): відео → MediaPipe → landmarks → SQLite (кожен кадр)
Analyzer (Python + C++ core): полить SQLite → сигнальний аналіз → патерни + підрахунок → events назад в SQLite

C++ core містить тільки чисту математику: геометрія, сигнальна обробка, DTW, state machine. Без залежностей крім STL.
Pose detection — на Python шарі через MediaPipe.
Demo (CLI + web) — Python (FastAPI + WebSocket).
Cross-platform compilation через CMake + scikit-build-core.

Чому так
Підрахунок повторень — це пошук періодичних патернів у часовому ряді. ML не потрібен.
Bottleneck pipeline — MediaPipe. C++ core виправдано архітектурно (port на iOS/Android, swap pose backend).
Protocol-based interfaces дозволяють iOS-розробнику swap-нути MediaPipe на нативний Vision framework.


Стек
Шар               | Технологія
C++ standard       | C++17
Build              | CMake 3.20+
Python bindings    | pybind11 (header-only)
Python build       | scikit-build-core
C++ tests          | doctest (header-only)
Python tests       | pytest
Pose detection     | mediapipe (Python)
Camera/video I/O   | opencv-python
DB                 | SQLite (WAL mode)
Web demo           | FastAPI + uvicorn + websockets
CLI demo           | argparse + multiprocessing
CI                 | GitHub Actions (linux x86, linux arm64, windows, macos)


Структура репо
exercises-counter/
├── CLAUDE.md
├── README.md
├── pyproject.toml
├── CMakeLists.txt
│
├── cpp/
│   ├── CMakeLists.txt
│   ├── include/exco/          # namespace exco
│   │   ├── geometry.hpp       # distance, angle, normalize (header-only)
│   │   ├── signal.hpp         # smooth, autocorrelate, find_period
│   │   ├── pattern.hpp        # Pattern, DTW, extract_cycle, serialize
│   │   ├── counter.hpp        # RepCounter (Schmitt trigger)
│   │   └── analyzer_core.hpp  # AnalyzerCore — orchestrates pipeline
│   ├── src/
│   ├── bindings/python_bindings.cpp
│   └── tests/                 # doctest
│
├── python/exco/
│   ├── __init__.py
│   ├── pose/
│   │   ├── base.py            # Protocol PoseDetector + Landmark
│   │   └── mediapipe_backend.py
│   ├── db.py                  # SQLite schema + read/write (WAL)
│   ├── writer.py              # Video → pose → DB
│   ├── analyzer.py            # DB → C++ core → events → DB
│   └── events.py              # @dataclass ExerciseEvent
│
├── demo/
│   ├── cli.py
│   └── web/
│       ├── server.py
│       └── static/index.html
│
├── tests/
├── docs/
│   ├── BUILD.md
│   ├── ARCHITECTURE.md
│   └── TUNING.md
│
└── .github/workflows/build.yml


Алгоритм (автодетекція патернів)
1. Pose detection: MediaPipe → 33 landmarks (x, y, z, visibility) на кадр.
2. Sliding window: ~2 сек (60 кадрів при 30fps) по y-координаті кожного joint.
3. Dominant joints: знаходимо 5 joints з найвищою дисперсією серед MAJOR_JOINTS (11-16 руки, 23-28 ноги). Face/hands/feet ігноруються — занадто шумні.
4. Low-visibility joints: якщо visibility < 0.5, тримаємо попереднє значення (hold last known).
5. Composite signal: усереднюємо сигнали домінантних joints + smooth.
6. Autocorrelation: шукаємо період повторення.
7. Pattern extraction: вирізаємо один цикл, нормалізуємо до 0..1.
8. DTW matching: порівнюємо з відомими патернами через Dynamic Time Warping. Додатково — перевірка overlap dominant joints (≥50%). Різні частини тіла = різна вправа.
9. Pattern enrichment: при кожному матчі оновлюємо signature (80% old + 20% new) і розширюємо dominant_joints.
10. Якщо збіг → Schmitt trigger counter рахує реп.
11. Якщо новий → зберігаємо як новий патерн, count = 1.
12. Pattern lifecycle: трекаємо active_pattern_id. Якщо патерн зʼявляється після паузи (30 frames без матчу) — генерується подія pattern_started.

Правила порівняння: різна амплітуда = різний патерн, різна швидкість = різний патерн, різні частини тіла = різний патерн.

Потік подій (розширений):
{pattern_id: int, count: int, timestamp: float}
- count=0 означає "pattern started" (відомий патерн відновився)
- pattern_id=-1 означає "new pattern detected"


Принципи коду
C++ core:
- Жодних залежностей крім STL.
- snake_case для функцій, PascalCase для типів.
- Header-only для math utils.
- Коментарі англійською (public API).

Python шар:
- Protocol для інтерфейсів (PoseDetector).
- @dataclass(frozen=True) для подій.
- Type hints скрізь.
- Async тільки в web (FastAPI). Pipeline синхронний.

Загальні:
- YAGNI. Не ускладнюй.
- Без зайвих бібліотек.


Як запускається
# Збірка:
pip install -e ".[web]"

# CLI на відеофайлі:
python -m demo.cli path/to/video.mp4

# CLI з камери:
python -m demo.cli --camera 0

# Web demo (з відео):
python -m demo.web.server path/to/video.mp4
# → http://localhost:8000

# Web demo (з камери, live):
python -m demo.web.server --camera 0
# → http://localhost:8000 (stick figure + dashboard + real-time counting)

# C++ тести:
mkdir build && cd build && cmake .. -DEXCO_BUILD_TESTS=ON && cmake --build . && ./cpp/exco_tests

# Python тести:
python -m pytest tests/ -v


Тюнінг
AnalyzerConfig параметри (описані в docs/TUNING.md):
- window_frames (60) — розмір вікна (~2 сек при 30fps)
- min_period (10) / max_period (60) — діапазон циклу
- period_strength (0.3) — поріг автокореляції (знижений для швидшої детекції)
- dtw_threshold (0.8) — макс DTW дистанція для матчу (знижений для розрізнення вправ)
- min_visibility (0.5) — мінімальна видимість joint для врахування
- counter_down (0.3) / counter_up (0.7) — Schmitt trigger пороги
- counter_min_frames (3) — антидрижання
- smooth_window (5) — вікно згладжування

Web demo (dashboard):
- Stick figure: canvas з скелетом (тільки major joints, без обличчя)
- Картки: Total Reps, Patterns, Duration, Reps/min + per-exercise
- Timeline chart: графік повторень у часі
- Pattern started toast: сповіщення при відновленні відомого патерну
- WebSocket /ws/landmarks — стрім landmarks для stick figure
- WebSocket /ws/events — стрім подій підрахунку
- GET /api/stats — статистика сесії


Робота з замовником
Репо: https://github.com/vGubriienko/exercises-counter
Замовник з iOS-екосистеми. Protocol для PoseDetector — обов'язковий для swap на Vision.
Тестове відео: https://www.youtube.com/watch?v=FEMGhHk2_ks


Що не робити
- Не додавати ML/нейронки в core.
- Не тягнути numpy в C++ core.
- Не робити threads/async в core.
- Не використовувати auto в публічних API C++.
- Не комітити згенеровані файли (build/, *.so, __pycache__, .venv).
- Не пушити нічого автоматично.
