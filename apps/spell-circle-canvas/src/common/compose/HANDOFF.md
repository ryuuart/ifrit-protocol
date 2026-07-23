# Handoff — SigilCompose study program

Written 2026-07-22 at the token limit. Everything below is either
committed or explicitly marked as in-flight. Workspace:
`/Users/long/.claude/jobs/440b1a2b/tmp/run2/`.

---

## State: all three user visual reports are CLOSED

| Report | Verdict | Where |
|---|---|---|
| aero "glass shouldn't be opaque" | NOT a broken backdrop. `glassTint` base Sky wash was srcOver **0.54**, washing out 54% of a blur that had been rendering correctly all along. Now 0.30. | `12b15e9`, ScenesAero.h:122 |
| y2k "incorrect blending layer on top" | REAL. Auto-promotion bake recorded at 1x, replayed at 2x — 283,577 px. Fixed; fresh render clean. | `f3bab39` |
| black_watch "incorrect blending layer on top" | NOT a blend bug. Captured at a fixed t=6.0 s = loom 0.75 = dead centre of the WEATHERED shade card. Right frame of the wrong beat. | `61c8963`, ROADMAP §31 |
| "overall performance degradation" | Closed. Promotion is INERT on GPU (profiler measures op-RECORDING under Graphite), now defaults off there. | `6770b08`, §29 |

**The pattern worth carrying forward (ROADMAP §31):** two of these named a
mechanism, and in both cases the mechanism was innocent. From a still, a
wrong-looking frame and a wrongly-CHOSEN frame are indistinguishable.
**Rule: when a report names a cause, verify the cause is even involved
before fixing it.** One A/B or one pixel sample, before any edit.

---

## IN FLIGHT — pick these up first

### 1. Cache::Group (maint agent, greenlit, correctness half written)

The last dual-backend failure. §30 has the spec, written by experiment.
Uncommitted in the tree at handoff time (7 files: `ComposeRuntime.h`,
`Composer.cpp`, `Paint.cpp`, `Compose.h`, `sketch_main.cpp`,
`kumiko_asanoha.cpp`, `ComposeTest.cpp`). **Do not discard — that is real
work.** The agent holds `Compose.h`, `Paint.cpp`, `kumiko_asanoha.cpp`.

- Bake path is FREE: whole-subtree device promotion (Paint.cpp ~1521)
  already flattens a rotated/blended subtree into an unrotated device
  layer. `Cache::Group` = trigger it from `cacheMode == Group`.
- The RISK is entirely in invalidation: kumiko's 523 strips are volatile
  via live opacity/scale BINDINGS, not content. Bake only while settled,
  drop the instant one ticks — §17's `scalarMemo` generalised to a
  subtree.
- **Target: 115.69 -> ~4.95 GPU work-ms (23x), measured by exhausting
  the authoring routes.**
- Bar set for it: pixel-identity across the full 6.4 s loop INCLUDING the
  transitions, a positive control that defeats drop-on-tick and requires
  the test to FAIL, a mechanism test via `profiledUnder()`/`requireRow()`,
  GPU verification with a control binary IT built, and CPU raster numbers
  too (fallout2 precedent: a sticking cache is not a free cache).
- It planned three measurement points: before / feature-present-but-not-
  opted-in / feature-on. **The middle one matters most** — it separates
  "does this help kumiko" from "does the memo tax every scene that never
  asks for it."
- **A contended baseline (kumiko 111.88 GPU, ~22:25–22:30) is QUARANTINED
  — the machine was never quiet in that window.** Re-take before/after
  back-to-back on a clear machine; that protocol is what caught fallout2.

### 2. The capture-time audit (mechanism landed, audit unfinished)

`--capture-at S` sweeps the corpus at a chosen scene time. Pass 1 (6.0 s)
completed all 56 scenes; **pass 2 (7.0 s) was at 16/56 when the session
ended.** Reproduce with:

```sh
build/bin/Release/ComposeGallery.app/Contents/MacOS/ComposeGallery \
  --headless /tmp/sweep60 --capture-at 6.0
build/bin/Release/ComposeGallery.app/Contents/MacOS/ComposeGallery \
  --headless /tmp/sweep70 --capture-at 7.0
# then diff pairwise; whatever differs was moving under the shutter
```

**Partial result, 19 pairs — 18 of 19 scenes are in motion at capture:**

| scene | % pixels differing 6.0 vs 7.0 |
|---|---|
| daemon console | 98.68 |
| ui_particles | 88.61 |
| flourish | 67.48 |
| zellige | 62.28 |
| aero desktop | 31.50 |
| persona menu | 10.82 |
| botanical | 8.64 |
| manuscript | 8.33 |
| load | 6.84 |
| passive tree | 6.82 |
| motion poster | 6.39 |
| nineslice | 4.44 |
| organic | 2.97 |
| tilemap | 1.65 |
| kinetic card | 1.36 |
| world hud | 1.36 |
| y2k chrome | 0.66 |
| night network | 0.42 |
| **beethoven** | **0.00 — the only settled plate** |

**Differing does NOT mean wrong.** The judgement is per scene and it is
the whole remaining task:

- **Continuous ambient motion** (daemon console's scrolling log,
  ui_particles, flourish) — any frame is representative. Leave alone.
- **Discrete named states** (black_watch's five registered shade families)
  — exactly one is canonical, and capturing another makes the plate assert
  something false. These need `ctx.captureAt()`.
- **Entrances that settle into a hold** — should be captured in the hold,
  not mid-entrance. Check anything in the 5–35% band first; that range is
  where a large but non-ambient change lives.

Only `black_watch` has been given a declaration so far (7.2 s, loom 0.90,
inside the Modern hold).

---

## Still open, lower priority

- **persona menu**: the selection wedge occludes the "EQUIP" label.
  Confirmed still present in the current build. Wants a P3R reference to
  fix properly — in the real menu the selected entry sits on its own
  slanted plate and neighbours are pushed clear, so this is a spacing
  decision, not a paint bug.
- **stroke_atlas**: ~40 dead `.absolute()` calls (all six edge setters set
  the flag; 1,327 were removed corpus-wide).
- **Residual 1-LSB background diff** on y2k/aero — flagged benign, not
  treated.

## Standing rules earned this session (full taxonomy in STATUS.md, ROADMAP §26–§31)

1. A documented limit is a CLAIM — test it or label it (§28).
2. The control has to be a thing you BUILT, not a thing you found. Four
   stale-artifact errors this session, the last one inside the measurement
   of a fix for a measurement problem.
3. Zero steady-state cache writes predicts THRASH, not BANDWIDTH. A
   sticking cache is not a free cache (fallout2).
4. `set -o pipefail` — a piped build hides its exit code.
5. Use absolute paths; background shells reset cwd.
6. A suite that passed is a claim about a BINARY, not a source tree.
7. When a visual report names a cause, verify the cause is involved (§31).
