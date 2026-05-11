# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.


Що це за проєкт
Прототип системи автоматичного підрахунку повторень фізичних вправ за відео. Джерело — realtime video stream (камера) або відеофайл. Система автоматично знаходить повторювані патерни руху без попередньо визначеного списку вправ.

Вихід — потік подій:
{pattern_id: int, count: int, timestamp: float}

Патерни зберігаються одразу в БД і розпізнаються з перших рухів при наступному запуску.
При відновленні відомого патерну генерується подія з count=0 (exercise started/resumed).

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


Доменна модель
Вправа = патерн. Один рух (підйом руки, присідання, мах ногою) — це і є вправа.
Немає окремого рівня абстракції "вправа" над патерном. Патерн ≡ вправа.
Не групувати патерни у вищий рівень. Кожен унікальний рух — окрема вправа.


Алгоритм (автодетекція патернів)
1. Pose detection: MediaPipe → 33 landmarks (x, y, z, visibility) на кадр.
2. Sliding window: ~2 сек (60 кадрів при 30fps) по y-координаті кожного joint.
3. Dominant joints: знаходимо 5 joints з найвищою дисперсією серед MAJOR_JOINTS (11-16 руки, 23-28 ноги). Face/hands/feet ігноруються — занадто шумні.
4. Low-visibility joints: якщо visibility < 0.5, тримаємо попереднє значення (hold last known).
5. Composite signal: усереднюємо сигнали домінантних joints + smooth.
6. Autocorrelation: шукаємо період повторення.
7. Pattern extraction: вирізаємо один цикл, нормалізуємо до 0..1.
8. DTW matching: порівнюємо з відомими патернами через Dynamic Time Warping. Додатково — перевірка overlap dominant joints (≥50%). Різні частини тіла = різна вправа.
9. Pattern enrichment: при кожному матчі C++ core оновлює signature (80% old + 20% new). dominant_joints фіксуються при створенні.
10. Якщо збіг → Schmitt trigger counter рахує реп.
11. Якщо новий → зберігаємо як новий патерн, count = 0 (pattern started).
12. Pattern lifecycle: трекаємо active_pattern_id. Потрібно 10 consecutive matches (pattern_switch_frames) для зміни активного патерну. Пауза 90 frames (~3 сек) = pattern stopped, при відновленні — подія з count=0.

Правила порівняння: різна амплітуда = різний патерн, різна швидкість = різний патерн, різні частини тіла = різний патерн.

Потік подій:
Exercise events (WebSocket type="exercise"):
{pattern_id: int, count: int, timestamp: float}
- count=0 означає "exercise started" (новий або відновлений)
- count>0 означає "rep counted" (Шмітт-тригер спрацював)

Routine events (WebSocket type="routine"):
{routine_id: int, sequence: [int], sets: int}
- Генерується коли послідовність вправ повторюється ≥2 рази
- Routine фіксується при першому виявленні, далі тільки sets інкрементується
- RoutineDetector не персистується — перебудовується з подій при кожному /api/stats


Команди

# Збірка (встановлює C++ extension + Python package):
pip install -e ".[web]"

# CLI на відеофайлі:
python -m demo.cli path/to/video.mp4

# CLI з камери:
python -m demo.cli --camera 0

# Web demo:
python -m demo.web.server path/to/video.mp4       # з відео
python -m demo.web.server --camera 0               # з камери, live
# → http://localhost:8000

# C++ тести (всі):
cmake -S . -B build -DEXCO_BUILD_TESTS=ON && cmake --build build && cd build && ctest --output-on-failure

# C++ тести (один test case):
./build/cpp/exco_tests -tc="назва тесту"

# Python тести (всі):
python -m pytest tests/ -v

# Python тести (один файл / один тест):
python -m pytest tests/test_pipeline.py -v
python -m pytest tests/test_pipeline.py::test_name -v

# Type checking:
mypy python/exco/


Тюнінг
Параметри AnalyzerConfig описані в docs/TUNING.md.


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
- Не затирати коміти соавтора (rebase/force-push заборонені на спільних гілках).
- Ніколи не додавати Co-Authored-By Claude в коміти.
- При конфлікті — не авторезолв. Ручний мердж після pull request.
