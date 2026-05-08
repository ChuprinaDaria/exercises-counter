# Routine Detection — Spec

## Model

Two-level counting system:

### Level 1 — Exercises (existing)
Individual repeating movements detected by autocorrelation + DTW.

- Arm raises = Exercise A
- Squats = Exercise B
- Bends = Exercise C

Each exercise tracks its own **count** (how many up-down cycles the Schmitt trigger counted).

### Level 2 — Routines (new)
A sequence of different exercises that repeats as a whole.

If the user does: A → B → C → A → B → C — that's routine [A, B, C] performed **2 sets**.

## Definitions

| Term | Meaning | Example |
|------|---------|---------|
| **Exercise** | One type of movement (detected automatically) | Arm raises |
| **Count** | How many repetitions within an exercise (each up-down cycle) | 12 arm raises = count 12 |
| **Routine** | Ordered sequence of different exercises | [Arm raises, Squats, Bends] |
| **Set** | One full pass through the routine | Did all 3 exercises once = 1 set |

## Detection Rules

### Exercise level (existing, C++ core)
1. Sliding window → autocorrelation → find period
2. Extract cycle → DTW match against known patterns
3. Schmitt trigger counts each up-down transition
4. New pattern created after `new_pattern_delay` frames without match
5. Pattern switch requires `pattern_switch_frames` consecutive matches

### Routine level (new, Python)
1. Track **exercise transitions**: when active exercise changes to a different one
2. Build a sequence of exercise IDs: `[A, B, C, A, B, C, ...]`
3. Consecutive appearances of the same exercise collapse into one: `[A, A, B]` → `[A, B]`
4. Find the shortest repeating prefix in the sequence
5. Minimum 2 sets to detect a routine
6. **Routine locks on first detection** — once found, the sequence does not change
7. Further passes through the sequence increment the set counter

### What is NOT a routine
- Same exercise done twice with a pause (A → A) = 2 sessions of exercise A, not a routine
- Random non-repeating sequence (A → B → C → A → C → B) = no routine detected
- Sequence that changes mid-session (A → B → A → B → C) = routine [A, B] at 2 sets, C starts a new sequence

### Micro-movements
Small incidental movements should not create exercises.
Enforced by:
- `new_pattern_delay` (15 frames = 0.5s) — don't create patterns from noise
- `period_strength` (0.3) — autocorrelation must find real periodicity
- Dominant joints filter — only high-variance body parts

## Events

| Event | Trigger | Data |
|-------|---------|------|
| `exercise` (count=0, new id) | New exercise created | `{pattern_id, count: 0}` |
| `exercise` (count=0, known id) | Known exercise resumes after pause | `{pattern_id, count: 0}` |
| `exercise` (count>0) | Schmitt trigger fires | `{pattern_id, count: N}` |
| `routine` | Repeating sequence found or set incremented | `{routine_id, sequence: [A,B,C], sets: N}` |

## Dashboard Display

```
┌─────────────────────┐ ┌─────────────────────┐ ┌─────────────────────┐
│  Exercise #1        │ │  Exercise #2        │ │  Exercise #3        │
│  12                 │ │  8                  │ │  10                 │
│  Shoulders, Arms    │ │  Legs               │ │  Hips               │
└─────────────────────┘ └─────────────────────┘ └─────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  Routine   #1 → #2 → #3                               2 sets      │
└─────────────────────────────────────────────────────────────────────┘
```

The number on each card = exercise count (repetitions from Schmitt trigger).
Body parts shown as subtitle (Shoulders, Arms, Legs, Hips).
Routine banner appears only when a repeating sequence is detected (≥2 sets).

### Notifications (toast)
- "New exercise #1 detected" (green) — first time
- "Exercise #1 resumed" (yellow) — known exercise restarts
- "Routine: #1 → #2 → #3 — 2 sets" (blue) — routine detected or set incremented

## Implementation Location

| Component | Where | Language |
|-----------|-------|----------|
| Exercise detection | `cpp/src/analyzer_core.cpp` | C++ |
| Exercise events → DB | `python/exco/analyzer.py` | Python |
| Body part mapping | `python/exco/body_parts.py` | Python |
| Routine detection | `python/exco/routine.py` | Python |
| Dashboard | `demo/web/static/index.html` | JS |
| Event streaming | `demo/web/server.py` | Python |
