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

---

## `Cache::Group` (§30) — shipped, pixel-verified, **NOT TIMED**

Landed by maint at the end of the session. Read ROADMAP §30 first; it has
the full write-up. This section is only what the next person must not
assume.

### Done, and verified

- **The enum plumbing.** `Cache::Group` in `Compose.h` (inserted after
  `Texture`, so `None` renumbered — nothing serializes `Cache`, and every
  use site is a comparison), `CacheState::Group` for the profiler, the
  `--bench` switch case, both `cacheMode != Cache::Texture` guards in
  `Composer.cpp`, and the enum-coverage test.
- **The subtree value memo, and it does drop on tick.** Asserted, not
  argued: `GroupDropsTheBakeOnTheFrameABindingTicks` moves one binding and
  requires the profile row to stop reading `Group` on that exact frame,
  requires `texturesBaked == 0` on it (a re-bake per moving frame is the
  fallout2 shape), then holds the phase and requires the bake to come back
  with exactly one write. It sits under a `Cache::None` wrapper with
  `requireRow()`, so it cannot go vacuous.
- **The positive control WAS run.** The drop was defeated in `Paint.cpp`,
  rebuilt, and the suite re-run: three tests fail, the pixel tests at 238
  of 241 frames, worst 17815 pixels at peak 217/255, against an honest
  residual of 1847 at peak 2. Restored; `compose_test` is 321/321 green on
  a binary built at 23:0x by me.
- **Pixel identity on kumiko itself**: 0 differing pixels at seven phases
  across the 6.4 s loop, at 1400x1000, `.cache(Cache::Group)` on both
  `lattice()` and `frame()`. Verified with ComposeSketch `--frame` against
  a stripped copy of the same sketch, not against a stored capture.

### NOT done — and the reason, which matters

- **No performance number exists for this feature.** None. The 23x in §30
  is the manager's hypothesis from a per-strip experiment, not a result
  from `Cache::Group`. My one baseline reading (kumiko 111.88 GPU / 189.03
  CPU raster, 22:25–22:30) is **QUARANTINED** — the machine was under a
  continuous build/ctest/render load and then a corpus sweep for that whole
  window. Do not use it, and do not use it as half of a pair.
  **The next person takes before AND after back to back on a clear
  machine.** Both halves must sit on the same commit and the same thermal
  state; that is the only property that makes the delta mean anything.
- **CPU raster is unmeasured too**, and it is the one that could regress:
  the group bake is a large texture (kumiko's lattice bakes at roughly
  canvas size) and a large baked texture is SAMPLED every frame whether or
  not it is re-baked. That is exactly how fallout2's card cache stuck
  perfectly and still regressed GPU 11.5 → 15–20 ms. The thrash proxy
  (`0 cache writes`) is green here and proves nothing about bandwidth.
- **The "feature present but not opted in" measurement was never taken.**
  It separates "does Group help kumiko" from "does the memo machinery tax
  every scene that never asks for it". The second is invisible unless
  measured deliberately. The memo only walks the tree for nodes whose
  `cacheMode == Cache::Group`, so the expected cost is the two extra bools
  per node in `computeVolatile` — but expected is not measured.
- **The stretch goal was not attempted**: `sigillum_aemeth`,
  `thunder_fulu`, `thaumonomicon` all have kumiko's shape and none was
  tried.

### What the next person must not assume

1. **Do not assume "byte-identical" is achievable in general.** It is not,
   and the reason is now measured (ROADMAP §30): every existing pixel test
   in this file composites over opaque BLACK, where srcOver's destination
   term vanishes and an isolating bake cannot show its one error. Over a
   lit ground the residual is 1847/57600 pixels at peak 2/255, and it goes
   to exactly 0 when the reference is itself isolated in a layer. That is
   the standard `Cache::Group` holds, and `Cache::Texture` and automatic
   promotion have always had the same property without anyone noticing.
2. **Do not remove the bake-rect clip** in the group branch of `Paint.cpp`.
   Intersecting with `getDeviceClipBounds()` is worth peak 12 → 2 on
   content that overruns its canvas, which is most content this feature is
   for.
3. **`groupRootOK` must stay out of `memoized`** in `computeVolatile`. It
   is deliberately excluded so that `cacheHolds` stays false for a volatile
   group root and a ticking frame falls through to LIVE paint. Folding it
   in would let a dropped group replay a stale picture instead, which no
   pixel test over black would catch.
4. The refusal list in the header is tested (`GroupRefusesWhatItsMemoCannotSee`,
   `AMovingGroupRefusesTheBakeRatherThanRemakingIt`) — §28 discipline. Any
   new limit added to that doc comment needs a case in those tests.
