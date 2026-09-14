# Findings

## A pen's retained guest is promoted by a stopwatch, under a deterministic capture too

**What the code does.** `paintRetained` in `src/common/compose/draw/Draw.cpp`
keeps one `Composer` per pen call site, built by `Guest::adopt`, which
gives it a ticker and a clock and nothing else. That composer therefore
runs at the default `Composer::PromotionPolicy::ByCost`: a node whose
paint measures over the threshold for eight frames is baked and blitted
from then on, and its antialiased edges land one code value away from the
live paint.

Nothing reaches that policy. `CanvasSession` pins promotion off when the
session is opened deterministic, and Sketchbook's `--promotion` and
`--no-promotion` doors write the same field — all three on the SESSION's
composer. A guest composer is constructed later and lower, inside a paint
program the session's composer is already running, and no pin is carried
into it.

So the picture a guest draws is a function of how fast the machine
painted it. It is stable while the paint stays on one side of the
threshold, which is what makes it worse than noise: a scene agrees with
itself over any number of runs of one binary and then disagrees with
itself after a rebuild that changed nothing it draws — the shape of a
false positive the byte-identity sweep cannot tell from a real one.

**What it was evidently intended to do.** `SketchContext::deterministic`
is documented as covering the runtime's own measured decisions — "the
composer's stopwatch-driven texture promotion — is already held off under
this flag by the session that opened it" — and the promotion tier of the
plate ledger exists to exercise the promoter deliberately, from a run
that says so. A guest composer is the same runtime making the same
measured decision on the same capture, so it belongs under the same pin,
and the `--promotion` door should reach it too.

**The evidence.** `psx_doom_fire` draws the DOOM word as a retained guest
behind the flame (`pen.element(doomWord(), …)`). Its plate moved after a
rebuild that changed no drawing at all — the registration sweep — by
exactly 1 code value on 1627 pixels, and those pixels trace the word's
antialiased outline and nothing else. The current binary renders it
identically six times out of six; the picture the baseline holds is a
different one. Forcing the guest's policy in `Guest::adopt` decides which:
`Off` leaves the current picture unchanged (the word is painted live now),
`Eager` produces a third picture (every promotable node baked). The
session's own pin and both Sketchbook doors leave all three alone.

**What a test should assert once intent is restored.** Open a session
deterministic on a sketch whose retained guest is expensive enough to
cross the threshold, step it past the eight frames the stopwatch needs,
and assert that the picture equals the one the same sketch draws with the
guest's promoter pinned off — and that both differ from the eager one, so
the case fails if the pin silently stops reaching. The cheaper half of it
is a direct one: in a deterministic session, the policy a guest composer
answers with is `Off`.
