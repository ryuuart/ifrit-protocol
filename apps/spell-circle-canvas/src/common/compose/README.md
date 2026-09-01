# SigilCompose

A C++20 static library that turns immutable, value-typed descriptions of a
2D scene into pixels on an `SkCanvas` the caller owns. It runs flexbox
layout through Yoga — with text leaves measured and drawn by SigilWeave,
the sibling paragraph-layout library — diffs each new description against
a retained tree to find what actually
changed, paints in an explicit CSS-like stacking order, and automatically
caches subtrees it can prove are not changing. Animation is Choreograph
outputs and timelines, stepped by a clock the host owns.

It owns no window, no surface, no render loop and no thread. You call
`Composer::render()` when your data changes and `Composer::draw()` inside
whatever paint callback your application already has, and it honours the
canvas's current matrix and clip like any other draw.

The problem it exists for is the middle ground between a paragraph layout
engine and a whole document engine: box-level composition of real
typography and arbitrary Skia drawing, sized by flexbox rules with
baseline alignment, layered with explicit z-order and blending, cached
like a display list, animated at scene rate, and refreshed from data
without rebuilding the world.

---

## Writing a component

A component is a free function from your data to an `Element`. There is no
base class, no lifecycle, and no state inside the library — the state is
the argument.

```cpp
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/typography/Typography.h>

#include <ranges>
#include <vector>

using namespace sigil::compose;
using namespace std::chrono_literals;

/// Your data. Copyable and equality-comparable — that is the whole contract.
struct Channel {
  std::string id;
  std::u8string label;
  float level = 0;     // 0..1
  bool alarm = false;
  bool operator==(const Channel &) const = default;
};

Element meter(const Channel &c) {
  const SkColor4f ink =
      c.alarm ? hex(0xff5252) : hex(0x8fd0ff);
  return box()
      .row()
      .gap(10)
      .padding(12)
      .corners({6})
      .fill(hex(0x0e1218))
      .alignItems(Align::Center)
      // A mark on part of the boundary: L-brackets at every tangent break.
      .stroke(spans::corners(12), stroke(1.5f, Fill::color(ink)))
      .child(text(c.label, type({.size = 13, .color = ink})))
      .child(box()
                 .grow()
                 .height(6)
                 .fill(ink)
                 .transformOrigin(0.0f, 0.5f)
                 // The bar ramps because the DESCRIBED value moved. Nobody
                 // steps it; the reconciler sees the change and starts a
                 // motion that retargets from wherever the bar is now.
                 .scaleX(animate(to(c.level), Transition{.duration = 220ms})));
}

Element dashboard(const std::vector<Channel> &channels) {
  return box()
      .column()
      .gap(8)
      .padding(24)
      .fill(hex(0x05070a))
      .children(channels | std::views::transform([](const Channel &c) {
                  // memo() skips the describe call entirely while the props
                  // compare equal. key() is what the reconciler matches on
                  // across describes, so rows survive reordering.
                  return memo(c, meter).key(c.id);
                }));
}
```

The host side is three objects — a clock, a ticker and the composer —
which the host owns and wires together:

```cpp
sigil::weave::FontContext fonts = /* yours */;
motion::FrameClock clock;
motion::Ticker ticker;
Composer composer(ticker, fonts);       // both must outlive the composer
composer.setClock(&clock);
composer.setSize({960, 540});
composer.render(dashboard(model));

const double dt = clock.tick();
const bool moving = ticker.tick(dt);
composer.draw(canvas);
const bool again = moving || composer.dirty() || ticker.active();
```

SigilSketch bundles exactly those three lines behind its own session, so
a sketch declares a scene and never a loop. That is a convenience of a
host and not of this library — spell the objects out when the clock or
the ticker is shared with something else.

The other write path is a live binding — a `choreograph::Output` the host
mutates every frame, read straight out of paint with no `render()` call:

```cpp
choreograph::Output<float> spin{0.0f};
ticker.add([&](double) {
  spin = motion::phase(ticker.elapsed(), 6.0);   // a wrapping [0,1) phase
  return true;
});

box().rotate(bind(&spin).target(0, 360));
```

---

## The mental model

**An `Element` is a shared, copy-on-write description.** It is a value: you
build one with fluent setters, copy it, compare it, throw it away. Copying
an `Element` bumps a refcount; mutating one that is shared clones first, so
a description already handed to the composer can never be altered behind
its back. Hot fields live inline on the node; rare and kind-specific state
is pushed into out-of-line value-semantic blocks, so an absent feature
costs one null pointer.

**An `Instance` is the retained counterpart**, and you never see it: parent
and child pointers, the resolved description, a Yoga node, paint order,
text layout state, animated value slots, derived geometry, and every cache
slot. Elements are write-only. Reads target the composer, after layout —
`Composer::bounds`, `Composer::paragraphLayout`, `Composer::hitTest`,
`Composer::routesAt`, `Composer::stats`, `Composer::profile`. Querying a
description is not offered, because it would invent a second identity
system next to keys.

### The two write paths

There are exactly two ways to change what is on screen, and both are
*declared*:

1. **Describe** — `Composer::render()` and `Composer::renderSlot()`. This
   carries structure and discrete state. Children are reconciled by key,
   falling back to position among unkeyed siblings; keyed reconciliation
   *is* the child-swap API. There is no imperative node mutation, and that
   absence is deliberate: it is the door that would make every cache
   unsound.
2. **Bind** — store a pointer to a live `choreograph::Output` in the
   description and mutate it per frame. Bound properties are paint-only by
   contract. They never relayout, and the node's cached content replays
   under the new transform or the new value.

That split is the whole reason caching can be automatic. Volatility is
*derived from the declarations*, not sniffed at runtime, so "does this
subtree change" is a decidable property rather than a heuristic.

### Phase order

`render()` mounts or patches, as a recursive keyed reconcile. A memo
compares its captured environment snapshot and then the author's props
comparator; a hit reuses the previous payload without describing at all.
A structural equality check is the prune: equal means nothing is marked
dirty and no transition is applied, though children still reconcile.
Unequal means dirty marking up the tree, a Yoga style write, a text
content revision bump, and transition application. Paint order among
siblings is a stable sort by `zIndex` then declaration order. Then the key,
slot and edge indices rebuild.

`draw()` detects the backend and host scale, then runs layout: Yoga first,
then up to three convergence rounds of custom `layout()` schemes,
`centerAt` pins, and the derive phase, each of which may re-run Yoga.
Recordings whose baked geometry moved are invalidated. Derive resolves text
exclusions and connector/rail routing over flat edge lists, cycle-guarded.
`Element::flowAround` subtracts the target's SILHOUETTE when it declares one
— a `shape()`, a routed connector or rail — so text runs into a star's
notches and through an annulus, and its BOX when it declares none. A round
silhouette is subtracted analytically. The margin means the same standoff in
every case.
Released scalars are scanned and volatility computed in one walk. Then
paint runs, selecting a cache tier per node.

### Paint order inside a node

Fixed, and worth memorising, because several traps are just this list:

```
backgrounds · background span passes │ fill · echoes │ overlays │
content leaf │ children │ foregrounds · foreground span passes
```

Decorations dress the node's *outline*, so `clip()` does not clip them —
it bounds the fill, the content leaf and the children. A stacking context
forms on `zIndex`, opacity below 1, a blend mode, a transform, a clip, or a
layer effect, and children cannot interleave outside it: a component cannot
escape the z-order of the site it was composed into.

### Text fx

Motion inside a text leaf is a list of **tracks**. One `Track` is four
values — *which* glyphs (`Selector`), *what* deviation from rest
(`TextEffect`), *how* the beats spread (`Stagger`), and the master
`Animatable<float>` progress that drives it. `Element::fx` appends one;
several compose per glyph, with `GlyphMod` offsets and rotations adding
and scale and alpha multiplying.

```cpp
text(u8"ONE LINE, TWO MOVES", display)
    .fx({.effect = fx::rise(20), .stagger = stagger(unit::Word)})
    .fx({.where = sel::text(u8"TWO"),
         .effect = fx::waveLoop(),
         .progress = &phase});
```

**Units.** `Unit` is the granularity a selector slices and a cascade beats
over: `unit::Glyph`, `unit::Cluster`, `unit::Word`, `unit::Line`,
`unit::Sentence`. `unit::Cluster` is the default, and it is the one that
keeps text correct — a base letter and its combining marks are one unit
and never separate under a stagger.

**Selectors.** `sel::word`, `sel::words`, `sel::line`, `sel::sentence`,
`sel::range`, `sel::text` and `sel::regex` name a position in the text;
`sel::each` slices every unit of one granularity the same way, with
`Selector::take` and `Selector::drop` partitioning each unit exactly.
Combine with `|`, `&` and `!`. A default-constructed `Selector` addresses
everything. Selection is resolved once per (content, layout, selector) and
cached on the element; a pattern that does not compile selects nothing and
warns once.

`sel::style` is the odd one out and addresses the TREATMENT rather than a
position: every run a `rich()` value added under a style name
(`RichText::add` with a name resolved through a `sigil::weave::StyleSet`).

```cpp
text(rich(base).styles(set)
         .add(u8"gusting ").add(u8"soon", "term").add(u8", then rain"))
    .fx({.where = !sel::style("term"), .effect = TextEffect::variableAxis("GRAD", 900)});
```

A glossary set in one registered style stays addressable when the copy
changes, where naming the literal words means editing the selector every
time an author edits a sentence. It resolves through the run's TEXT, so
re-registering the name against a different style — or a `spanPaint` or
`spanStyle` cutting across the run — leaves the same runs selected. Only a
named `rich()` run carries a name: plain text, a run given a style
directly, and the paragraph overload have none, so there it selects nothing
and warns once per name, as does a name no run was written with.

**Cascades.** `Stagger` keeps the GSAP model — `eachMs` or `amountMs`,
`durationMs`, and a `Stagger::From` origin: `Start`, `Center`, `End`, a
seeded `Random` and a two-ended `Edges`. `Random` deals a scrambled EVEN
ladder — every unit takes a distinct rank, so no two units open together —
and it is deterministic: the ranking hash is keyed on the unit count and
`Stagger::seed`, so the same text scatters the same way on every frame and
after every relayout. At the default seed of 0 the key is the count alone,
which makes two same-count cascades scatter identically; give each field
its own nonzero seed for independent scatters. `Stagger::distribution`
shapes the start times across the cascade, and `Stagger::then` nests a
second cascade inside every beat of the first (`stagger(unit::Word,
{…}).then(unit::Glyph, {…})`).

**Irregular timing.** `cues` replaces the even spread with a TABLE — one
start time per unit, in ms — which is what caption, lyric and lip-sync
timing actually is:

```cpp
text(lyric).fx({.effect = fx::rise(12),
                .stagger = stagger(unit::Word,
                                   cues({0, 340, 720, 1180},
                                        {.durationMs = 180}))});
```

It returns a `Stagger`, so it goes anywhere one goes and compares like one.
A table says only *when unit k starts*; `over`, `durationMs` and `then` are
untouched by it, while `eachMs`, `amountMs`, `from` and `distribution` have
nothing left to say and are ignored. A unit past the end of `Stagger::cueMs`
starts at the last entry (the tail piles, visibly, rather than being given
times nobody wrote), entries past the last unit go unread, and either
mismatch warns once.

**Which list the beats are numbered against.** `Stagger::beatsOver` takes
`beats::Selection` — the default, numbering only the units the track's own
selector resolved — or `beats::Text`, numbering every unit of that
granularity in the paragraph, addressed or not. Two tracks that partition
one paragraph share a clock *by construction* only under `beats::Text`;
under the default they line up while their selections happen to resolve
lists of the same length and silently drift apart when they stop. A nested
cascade takes the outer one's answer, as it already takes the outer
`durationMs`.

**Reading the schedule back.** `Composer::beatsOf` reports the cascade one
track is actually running, after layout:

```cpp
for (const Beat& b : composer.beatsOf("lyric", 0))
  if (b.active) markTheWordAt(b.rect, b.localT);
```

`Beat::rect` is the unit's laid-out rect in the composer's coordinate space
— read off the placement, so it follows a wrapped line, a mixed-style run's
own size, a path baseline and a vertical column; `Beat::unitIndex` is the
outer unit the beat belongs to (a nested cascade reports several beats
sharing one, one per inner unit); `Beat::startMs` is the compounded delay;
`Beat::localT` and `Beat::active` are that beat's own progress right now.
This is what anything travelling WITH a cascade and made of something other
than glyphs — a bouncing ball, a playhead, an underline, a caret, a
per-unit meter — reads instead of restating `i * eachMs`, which stops
agreeing with the engine the moment the cascade nests or takes a table.
An unknown key or track index resolves to an empty vector, silently, like
the rest of the query family. For a run that is *not* in the tree,
`measureRun` and `runPens` are the static answer instead: `runPens` returns
one pen position per glyph plus a past-the-end entry, so `.back()` is the
run's laid-out width. A space between two words is a gap the flow leaves
rather than a glyph, so it rides the advance of the glyph before it — which
is what makes those sums reproduce the pen positions the layout used across
a whole sentence.

**The whole span.** A beat says when it *opens*; `Composer::cascadeSpanMs`
says when the whole schedule is *over* — the ms of virtual time the track's
master progress [0,1] maps onto: `durationMs + eachMs·(N−1)` for the flat
even ladder, `durationMs + amountMs` in amount mode, the compounded extent
under `Stagger::then`, and the latest time any unit reads plus `durationMs`
under a cue table. It is the number a progress duration must equal for a
cascade to run at its authored ms — a table's times are absolute only when
the window driving the track spans exactly the span — and the number
anything sequenced *after* the cascade offsets from. It is computed by the
same resolved cascade the glyphs and `beatsOf` read, so the three cannot
disagree; an unknown key or track index resolves to 0, silently.
`Stagger::spanMs` is the same number at *declare* time, computed from unit
counts alone for the site that needs it before any node exists — above all
the progress transition written right next to the stagger:

```cpp
const Stagger cascade{.eachMs = 28, .durationMs = 480};
const float span = cascade.spanMs(13);  // 480 + 28·12, before any layout
// Drive the track's progress over exactly `span` ms and the last glyph
// lands as the master arrives at 1. After a draw,
// composer.cascadeSpanMs("title", 0) reads the same number off the
// mounted track — with the unit count the laid-out text supplies.
```

For a nested cascade the second argument is how many inner units one beat
holds (the widest beat's count, where they vary), and an amount-mode span
is the same for every count past one, because the amount *is* the spread.

**The looping cascade.** `Stagger::loopMs` makes the schedule wrap: above 0,
every unit's beat re-opens on its own cycle of that period, phase-offset by
the unit's start time — even ladder and cue table alike — so steady
continuous motion (rain re-dropping column by column, arrivals that never
stop) is *declared* rather than faked by re-running a one-shot. The master
stays the one clock, and one sweep 0→1 is exactly one cycle: unit *i* reads
`clamp(((master·loopMs − startᵢ) mod loopMs) / durationMs)`, so master 0
and master 1 name the same instant of the cycle and a **wrapping bound
phase** — an `Output` stepped mod 1, the clock `fx::waveLoop` already reads
— drives it seamlessly forever:

```cpp
Stagger cascade = stagger(unit::Line, cues(columnStartsMs));
cascade.then(unit::Cluster, {.eachMs = 80, .durationMs = 1400});
cascade.loopMs = 5000;  // every column re-drops on its own cue, forever
text(field, rain).fx({.effect = streak, .stagger = cascade,
                      .progress = &phase});  // phase wraps every 5 s
```

Between its beat's close and its next opening a unit rests at local 1 — its
landed deviation — and returns to 0 the instant the beat re-opens, so an
effect that loops cleanly ends where nothing shows. Start offsets fold mod
the period (a start past `loopMs` lands at start mod `loopMs`), and the
fold means every unit is *always* somewhere in its cycle: there is no
"before the first beat", which leaves `fx::hold` nothing to veto (local
time touches 0 only at the instant of re-opening) — an effect on a looping
cascade gates its own arrival instead, the way a streak table's head is its
own entrance. `Composer::cascadeSpanMs` and `Stagger::spanMs` answer the
**period** — still the ms the master maps onto, and the number a driver's
wrap must span for the schedule to run at its authored ms. One loop governs
the whole cascade, read off the outer spec under `Stagger::then` as
`Stagger::beatsOver` is; `Beat::localT` reports the wrapped local time (the
same number the effect is handed) and no cycle index rides beside it — the
master is a phase mod 1, so cycle identity lives with whoever steps the
phase. Driving that phase is also what keeps the element live: a looping
cascade at a *constant* master is one still frame of its cycle, exactly as
a wave at one phase is, so permanent volatility is declared by the wrapping
binding, never by the field, and `loopMs = 0` — the default — is the
one-shot cascade.

**Marking the type.** `Element::mark` anchors a child to the rect a selector
resolves — a caret, a callout, a tick, a rule standing at a word's edge:

```cpp
text(line, style)
    .mark(sel::word(3), box().left(0).top(pct(100))
                             .width(pct(100)).height(2).fill(ink));
```

The child's box is that rect, and its own placement longhand is read
*inside* it, exactly as a `positioned()` child reads it against its parent —
so a mark with no dims at all simply is the unit's rect, and one with them
is free to hang outside it. That is the difference from `RichText::slot`,
which reserves space *in the flow*: the line breaks around a slot and the
type after it starts further along, where a mark is placed on a line laid
out as though it were not there. A selector resolving several units gives
one rect, the union of all of them; one resolving nothing places nothing and
warns once. The rect is the **rest** rect — where the layout put those
glyphs, not where a track has thrown them this frame — so a mark follows a
reflow and stands still under a cascade; read `Composer::beatsOf` and drive
the mark's own transform for one that must ride the motion. On a path run
(`onPath`) the rect is on the curve, at the run's *resting* placement — a
run driven along its baseline is a paint-time deviation like any track's.
A mark needs no
`reach`, being a child: the recording cull already grows by the union of a
node's children.

**Effects are comparable values**, which is what lets text carrying tracks
prune like any other static leaf. A preset compares by its name and its
parameters; an ad-hoc body goes through `fx::effect`, which takes the key
its author gives it — two different bodies under one key compare equal and
one of them silently never draws. The one declaration an ad-hoc body
carries, `TextEffect::displacing`, joins those parameters rather than
sitting beside them, so two bodies under one key that disagree about
placement do not prune onto each other. `fx::seq` remaps local time so each
phase sees a renormalised 0→1 (`TextEffect::until` sets the joint,
`Phase::xfade` lerps across it), and `fx::mix` evaluates several effects at
one time and composes them by the same algebra stacked tracks use.

**Keyframe tables.** Every published web or motion reference is a list of
(position, value) entries, and `fx::keys` is that list as an effect. A
`fx::Key` is a moment in local time, a `GlyphMod` at it, and optionally a
curve of its own:

```cpp
const TextEffect rubberBand = fx::keys({
    {0.00f, {}},
    {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
    {0.50f, {.scaleX = 1.15f, .scaleY = 0.85f}},
    {1.00f, {}},
}, &choreograph::easeInOutCubic);
```

The curve applies **per segment** — every pair of entries runs the whole
curve over its own span, which is what a keyframe list means and what one
curve stretched across the table would not be. `fx::Key::ease` overrides it
for the segment that *opens* at that entry; unset segments are linear.
Interpolation is componentwise through the same arithmetic a `fx::seq`
crossfade uses, so `codepoint` cuts at the middle of a segment and `axis`
lerps only between entries naming the same tag. The table is the identity:
two `fx::keys` over the same numbers and the same named curves compare equal
and prune, and a table declares its own reach from the offsets, growths and
leans it publishes. A sequence is not a table over effects and neither is the
other's special case — a `Phase` is an effect re-clocked over its window, a
`fx::Key` is one deviation standing still.

**Holding a beat.** `fx::hold` wraps an effect so a unit whose beat has not
opened paints *nothing*: a cascade hands a waiting unit a local time clamped
to 0, and an effect that deviates at 0 is already performing out of turn.
`fx::scramble` is the case that shows — it substitutes from local 0, so an
unheld glyph still waiting shows a *wrong* letter rather than no letter. The
hold is alpha 0 and not the identity, because the identity is a glyph sitting
at rest, which for a substitution is exactly the answer the effect exists to
withhold. Alpha multiplies, so a hold is a **veto**: a glyph whose held track
has not opened paints nothing however many other tracks have opened on it.
Put it on the track that owns the glyph's arrival. A *looping* cascade
leaves it nothing to veto — every unit is always somewhere in its cycle —
so there an effect gates its own arrival instead (the looping-cascade
passage above).

**Effects get an `Rng`**, seeded from the glyph's identity, so a scatter is
the same scatter on every frame and after every relayout — which is what
lets it settle and cache instead of jittering forever.

**A shader per letter is one pass.** `fx::pass` makes a track's effect a
PASS rather than a per-glyph deviation: the runtime renders the units the
track addresses into a layer and runs the material ONCE over it, handing
the track's own schedule in as uniform data — `uContent` (the layer),
`uUnitRect[]` (each unit's box, node-local px) and `uUnitPhase[]` (each
unit's cascade-local 0→1, then a stable per-unit seed) — so per-letter
treatment is data rather than scene structure, and the cost is one draw
plus one pass whatever the unit count is:

```cpp
// emberDissolve is a SigilMaterial recipe over the params struct Burn,
// carrying the pass body as its SkSL.
auto burn = Material::recipe(sigil::material::Material(emberDissolve,
                                                       Burn{ink}));
text(u8"EMBER DECODE", display)
    .fx({.effect = fx::pass(burn),
         .stagger = stagger(unit::Cluster, {.eachMs = 260})});
```

The material must be RECIPE-BACKED — `Material::recipe` over a recipe
carrying an SkSL body — because the unit count is baked into the compiled
shader: a runtime effect's array size is fixed at compile and SkSL has no
uniform-bounded loop, so the runtime holds a specialization of that recipe
per distinct count, its body the declarations above plus `const int
kUnitCount = N` ahead of the author's. Write the body against those names
and do not declare them, and declare every uniform of your own as a params
field rather than in the body's text; any other material warns once and
the track draws its glyphs at rest. `main(xy)` runs in the node's own px, the layer is sampled at the
device's resolution (a 2x host stays sharp with no supersampled bake), and
the pass is BOUNDED: it paints the node's box grown by the track's `reach`
and nothing outside it, unlike an `Element::effect` shader pass. The
per-unit rects and times are resolved from the SAME cascade
`Composer::beatsOf` reports, so a pass, a mark and the glyphs cannot
disagree about the schedule.

**A pass can declare where it rests.** `fx::pass(m).restsAt(0)`,
`.restsAt(1)` and `.restsAt(0, 1)` promise the SkSL is an EXACT
pass-through at those unit phases. When every addressed unit's resolved
local time sits on a declared phase the runtime skips the layer and the
shader and draws the glyphs directly — so a settled pass on a node that
repaints for unrelated reasons (an orbiting `onPath` ring) stops paying
for a shader that changes nothing. The promise is unverifiable, in the
family of `reach` and `bleed()`: declare a phase where the shader is not
a pass-through and the picture pops at the seam, with no diagnostic. The
test is exact — a one-shot cascade clamps a unit to exactly 0 before its
beat and exactly 1 after. A looping cascade touches 0 only at the instant
a beat re-opens, so `restsAt(0)` effectively never engages there
(correctly — the cycle is always mid-flight somewhere), while units rest
at exactly 1 between beats, so `restsAt(1)` engages whenever no beat is
mid-cycle. Undeclared, a pass always runs.

Order against everything else: deviation tracks apply FIRST, and the pass
reads the deviated pixels — a pass is post-processing, and pixels are what
it processes. A glyph a pass addresses draws only inside that pass's
layer, never directly as well; several pass tracks run in declaration
order, each over its own selection's layer, and a glyph two passes address
renders in both. A path baseline and a vertical column place glyphs before
any of this, so a pass rides both. A pass is a whole-track statement:
inside `fx::seq`, `fx::mix` and `fx::hold` its material is not consulted —
sequence a pass by driving its progress, and gate its onset in its own
SkSL, which holds the whole schedule.

**Colour as a cascade.** `fx::tint(from, to)` is the colour reveal — a
karaoke wipe, a highlight sweeping a word — and it carries one inversion
worth stating once. `GlyphMod::colorMul` MULTIPLIES, and a multiplier only
takes a colour toward black, so **the element is set in `to` and the effect
multiplies down toward `from`**. The arguments still read in time order and
the division is done inside: `fx::tint(pale, sung)` on a line set in `sung`
wipes pale to sung, while setting the line in `pale` draws pale throughout
with no diagnostic. Multiplying is also what lets it tint a gradient-filled
line without knowing what fills it, and why a destination channel of zero
cannot be departed from.

The way *up* is the other two colour terms. `GlyphMod::colorAdd` is the
**hard flash**: added to whatever the style paints — after the multiply,
clamped at the draw — it brightens where a multiplier can only darken, and
it *adds across tracks*, the sum clamping once, so two half flashes make one
full one. `GlyphMod::colorScreen` is the **phosphor glow that never clips**:
the painted colour c becomes 1 − (1 − c)(1 − s), lifting each channel in
proportion to its headroom, and screens combine *commutatively* across
tracks — stacked glows compose order-free. Both are RGB-only (coverage
stays the multiplicative lane's — `alpha` and the multiplier's own alpha),
both lerp componentwise in a `fx::keys` table like every other continuous
field, and both are usually spoken through one: a keys table that opens
bright and decays to zero is the flash-then-settle an entrance wants.
Because screening against a constant is affine per channel, multiply, add
and screen ride *one* memoized colour-matrix filter on a shader-filled
pass — no second filter form — and a flat pass takes the same arithmetic in
its colour. Neutral values (all zero) cost nothing: the untouched-paint
fast path is byte-identical to a deviation that never mentions them.

**What a `GlyphMod` can say.** Beyond `dx`, `dy`, `scale`, `rotateDeg` and
`alpha`: `colorMul` multiplies every pass the glyph's style draws (a flat
pass multiplies its colour, a shader pass takes an equivalent modulation,
so a gradient keeps its ramp and wears the tint over it); `colorAdd` and
`colorScreen` brighten over every pass the same way — the flash and the
glow of the tint section above; `scaleX`,
`scaleY`, `skewXDeg` and `skewYDeg` place the glyph with a full matrix,
because an RSXform carries a rotation and one scale and no shear at all —
the two shear angles read as `Element::skewX` and `Element::skewY` do, and a
glyph naming both takes one shear pair rather than one shear after the
other; `axis` drives a variable-font axis at draw time; and `codepoint`
draws a different letter in this one's place. The last two are SUBSTITUTIONS and compose
last-one-wins — a `fx::seq` crossfade cuts them at the middle of its window
rather than lerping, because there is no half-way glyph between two
outlines. (Two phases driving the *same* axis are the exception, and lerp.)

Both substitutions are GATED, because both keep the pen positions shaping
computed. `axis` is honoured only for an advance-invariant axis — the
runtime probes the face once per axis and refuses one that moves advances,
drawing at the shaped face and warning once. `codepoint` is honoured only
where the replacement has the original's advance ALONG THE AXIS ITS RUN
ADVANCES ON — the width along a line, the height down an upright column; a
swap that differs there would move every letter after it, which is a
reshape and not a redraw.
`TextEffect::variableAxis` holds a coordinate and `fx::variableAxisSweep` sweeps between
two across local progress
and `fx::scramble` is the decoding-text preset built on the substitution:
each glyph churns through a charset and resolves to the true letter by
`t = 1`, seeded per glyph so it is the same churn on every frame.

`Element::variationDrive` is sugar over a whole-text `axis` track, so a
driven axis composes with entrances and loops instead of being a second
text path they would hide.

**Snapping, and `Track::continuous`.** Rotation, alpha, the colour terms
(`colorMul`, `colorAdd`, `colorScreen`) and the axis coordinate are
quantized before they reach the
draw: each distinct value is a distinct batch bucket *and* a distinct
glyph-atlas strike. The axis ladder is cut per RENDERED SIZE — one step is
a fixed distance in the axis's design units, a design unit displaces an
outline by a fixed fraction of the em, and that fraction is more pixels the
larger the glyph is drawn — so a headline gets a proportionally finer
ladder than a caption and does not have to reach for the opt-out to look
smooth. Set `Track::continuous` where the steps still show and pay for it:
a continuous coordinate has no bounded set of faces, so its clone is built
fresh and its glyphs rasterized fresh every frame, and nothing retains it.
A glyph any addressing track declares continuous is continuous.

**Every track declares its `Track::reach`** — how far past the element's
box it may throw a glyph — or takes the number its effect declares. The
recording cull grows by it, on the same over-reporting-is-safe contract a
decoration's `bleed()` carries. Under-report and cached output is
truncated with no diagnostic.

`Element::textFill` and `Element::textStroke` combine with tracks and with a
path baseline alike: a letter in flight, and a letter on a curve, are painted
with the same glyph paint a resting one is. `Element::echo` skips fx text by
contract.

### Text on a path

`Element::onPath` makes a `TextPath` the run's BASELINE. The run is shaped
once — real kerning, real ligatures, real advances — and then laid out
through SigilWeave's own contour geometry: every contour of the resolved
`TextPath::path` is one interval of the run's one line, and the words fill
them in order.

```cpp
text(u8"SIGILLVM · DEI · AEMETH", inscription)
    .width(320).height(320)
    .onPath({.path = shapes::circle(),
             .at = &phase,                       // the marquee
             .align = TextPath::Align::Center,
             .orient = TextPath::Orient::Tangent})
    .fx({.effect = fx::rise(18), .stagger = stagger(unit::Cluster)});
```

**`at` is where along the baseline the run sits**, as a fraction of the whole
path's length with the contours chained end to end — which is what lets seven
chords of a heptagon carry seven captions addressed by fraction alone.
`TextPath::align` measures the run against that point: `Start` begins there,
`Center` centres on it, `End` finishes there. It is an `Animatable<float>`,
so every `bind()` and `animate()` verb applies, and on a CLOSED baseline the
fraction WRAPS — a phase output running 0→1 forever is the infinite marquee,
with no seam. Moving it is PAINT-ONLY: the run is shaped and broken across
the contours once, and the phase re-places glyphs that were already placed,
so a marquee costs a repaint and never a reflow. It declares content
volatility while it runs and releases once it provably holds still.

**A CONTOUR BOUNDARY IS A BREAK.** A word that does not fit the contour it
reached starts the next one, rather than bending across the gap between two
disconnected curves. A run that outlasts the last contour simply stops, and a
run pushed off the end of an open baseline by its phase drops the glyphs that
ran off rather than piling them on the last point.

**`fx()` and `onPath()` compose; neither wins.** THE BASELINE PLACES THE
GLYPH, THEN THE TRACKS DEVIATE FROM THAT PLACEMENT, IN THE FRAME THE BASELINE
PUT IT IN. On a curve that means `fx::rise` lifts a letter off the CURVE
along its own local perpendicular rather than straight up the canvas, a
stagger's shove stays tangential to the lettering it belongs to, and a
track's rotation adds to the tangent the glyph was already turned to. Scale,
alpha, the colour multiplier and both substitutions are per-glyph dressings
and are untouched by the frame — so `variationDrive` and `fx::scramble` reach
curved lettering exactly as they reach straight lettering.

`Element::textFill` and `Element::textStroke` reach a path run like any
other, with one caveat: a metric-mapped material maps its unit square to the
run's STRAIGHT metric band, which is not where the type ended up. A flat
colour and a stroke are exact; a gradient across a ring is not what it
looks like.

`TextPath::orient` is `Tangent` (running lettering), `Radial` (the baseline
along the radius, for an astrolabe limb or a compass rose) or `Upright`
(level everywhere, for a calendar ring). `autoFlip` turns the RUN over once
so lettering on the lower half of a ring reads right way up — never each
glyph, which would reverse the reading order. `TextPath::offset` rides the
type off the baseline, positive to the LEFT of travel. Tangents snap to a
ladder of directions because each distinct rotation is a glyph-atlas
strike, and the ladder is cut per RENDERED SIZE — one angular step sweeps
a bigger glyph's extremity through more pixels, so display lettering on a
turning ring gets a proportionally finer ladder and does not tick letter
by letter as a marquee turns; `TextPath::exactTangent` is the opt-out,
for artwork that must hold the exact angle.

**A RUN IN MOTION PLACES ITS GLYPHS ON THE SUBPIXEL GRID; a run at rest
keeps whole-pixel origins.** A glyph mask is rasterized for a quantized
origin, so a ring creeping along by a fraction of a pixel per frame does
not creep at all on whole pixels: every letter stands still until its own
origin crosses a pixel boundary and then hops a whole one, at its own
moment. Nothing about the placement arithmetic causes it and no ladder
fixes it. Three declarations put a run on the finer grid, all of them the
question "does what this run draws land somewhere else next frame": a
BOUND or animated `TextPath::at`; a bound or animated `rotate()` (or
any other geometric transform) at or above the text node; and a live
`fx()` track whose effect moves glyphs. A phase written
as a plain number, or a figure turned by re-describing a literal angle,
declares nothing and is treated as type at rest. The grid is read off the
declaration and never off a frame-to-frame difference, so a marquee parked
at a phase keeps the placement it was turning with rather than taking one
last quarter-pixel shift the moment it settles.

**A track declares through two facts, and needs both.** Its progress must
be live — bound, or mid-transition — and its effect must actually move
glyphs, which is what `TextEffect::displaces` answers. That answer is
*inferred* almost everywhere: a preset knows its own deviation (`fx::rise`,
`fx::slide`, `fx::pop`, `fx::spinIn`, `fx::scatter` and `fx::waveLoop`
move glyphs; `fx::typeOn`, `fx::variableAxisSweep`, `fx::tint` and `fx::scramble` touch
coverage, colour or the outline and leave every pen position alone),
`fx::keys` reads its own table (any entry publishing an offset, a lean, a
shear or a growth), and `fx::seq`, `fx::mix` and `fx::hold` derive from
their operands. `fx::pass` does not displace — its shader runs over pixels
already rasterized at the resting origins, so refining those origins says
nothing about where the pass puts its output. Only `fx::effect` has to be
told, because a lambda is opaque until it runs: it assumes the moving
answer, and `.displacing(false)` is the author's promise otherwise. A
karaoke wipe, a decoding scramble and a staggered fade therefore keep
whole-pixel origins and their bytes however hard they run, and a settled
displacing track goes back to them — its glyphs are standing somewhere
else and standing still.

**The baseline declares its own reach.** A resolved path is not bounded by
the node's box — a custom `Shape` may return a curve well outside it, and
`offset` rides the type further off again — so the cull grows by the curve's
bounds plus the glyph band and whatever the tracks reach, the same
over-reporting-is-safe contract `bleed()` and `reach()` carry. Nothing to
declare by hand; it follows from the baseline you gave it.

### Mixed text

**There is no markup language.** Text that is not all set the same way is a
`RichText` value plus selector styling, and the two cover different halves
of the problem: `rich()` says what the CONTENT is, and `spanPaint` /
`spanStyle` say what a RANGE of it looks like.

```cpp
auto p = rich(base)
             .add(u8"Signal ")
             .add(u8"woven", accent)
             .add(u8" through ")
             .add(u8"noise", mono);

text(p)
    .spanPaint(sel::regex(u8"[0-9]+"), sigil::weave::PaintStyle(SK_ColorRED))
    .maxLines(3)
    .ellipsis(u8"…");
```

`RichText::slot` reserves an INLINE SLOT in the run stream — a box of blank
space the flow weaves in, and the name a child of this text node is laid out
into:

```cpp
text(rich(body).add(u8"press ").slot("key", {28, 18}).add(u8" to continue"))
    .child(box().key("key").fill(ink).corners({4}));
```

The reserved box is one UNBREAKABLE word: no line breaks inside it, and it
opens its line when it is taller than the type. The child is an ordinary
subtree — it animates, caches and hit-tests like any other element — and it
re-lands wherever the placeholder lands when the text reflows. It is a
POSITIONED subtree: the placeholder rect is its box, so flex layout does not
run inside it and its own children take explicit rects, exactly as under
`positioned()`.

**A TEXT SLOT IS NOT A MOUNT SLOT.** `slot()` and `Composer::renderSlot` name
a hole a HOST fills from outside the description, and those names live in one
registry for the whole composition. These names live in the rich-text value
alone and are matched against the `key()` of that text node's own children —
so two captions may both reserve a slot called `"icon"` without colliding,
and neither is reachable by `renderSlot`. A child keyed for a slot the
content does not declare draws nothing and says so once; a slot the geometry
could not place is silent, like every other word that did not fit.

`RichText::add` takes a run in the base style, a run in its own
`sigil::weave::TextStyle`, or a run under a NAME resolved through a
`sigil::weave::StyleSet` — supplied by `RichText::styles` or inherited
through `env::Provide`. An explicit set beats the inherited one whichever
order the two are written in, and a name the set does not register resolves
to the base `rich()` was given, so a misspelling shows as content set in
the default rather than as content that did not draw. `RichText::runs` and
`RichText::base` read the finished value back.

**It is a comparable value, and that is the point.** Two rich texts with
the same base and the same runs in the same styles are equal, so a
component that rebuilds its spans every describe prunes exactly like a
static leaf. The `text(std::shared_ptr<sigil::weave::Paragraph>, options)`
overload cannot answer that question — a fresh pointer is a fresh identity
and reads as changed content every time — which is why it stays the escape
hatch for the passage too custom for either verb, not the way to set two
colours in a sentence.

**Selector styling.** `Element::spanPaint` and `Element::spanStyle`
restyle whatever the SAME `sel::` selectors the tracks use address, on
every content form alike — plain text, `rich()` spans and the paragraph
overload. They are ordered by **what they are allowed to disturb**:

| verb | changes | re-shapes |
| --- | --- | --- |
| `spanPaint` | paint alone — a colour, a shader, an underline, a glow pass | never |
| `spanStyle` | anything a `sigil::weave::TextStyle` holds | the words its range covers — unless the only change is advance-invariant axes |

Both are ordered lists — a LATER DECLARATION WINS on overlap, so a broad
rule followed by a narrow exception reads in the order it is written — and
both are comparable values, so a re-described list prunes and only a
changed one re-resolves.

That rule holds **per dimension** where the two verbs meet. The paint of a
range is `spanPaint`'s to say, so a `spanStyle` over text an earlier
`spanPaint` coloured applies its other dimensions and leaves that colour
standing: either declaration order draws the same passage, and neither
verb has to know what the other declared. A `spanStyle` with no
`spanPaint` under it paints with the style it is given, as ever.

The middle ground is `spanStyle`'s own. A style that differs from the text
it covers only in variable-font axes the face carries advance-invariantly
does not re-shape: a grade is advance-invariant *by construction* — it
thickens a letter without moving the letter after it — so it is exactly
the restyle that can keep the layout the paragraph already has, and the
restyle keeps it:

```cpp
sigil::weave::TextStyle graded = base;
graded.variation("GRAD", 780);
text(copy, base).spanStyle(sel::regex(u8"[0-9]+"), graded);
```

Such a restyle is carried as a track holding `TextEffect::variableAxis`,
and inherits what that means. The coordinate is a `GlyphMod::axis`, so it
goes through the same size-scaled ladder a driven axis does and composes
with entrances and loops instead of being hidden by them; and the leaf
then draws through the batched glyph path, which paints glyphs and not a
span style's underline or strikethrough. Anything else the style changes —
another face or size, an axis the face moves advances on, an axis the text
was shaped with and the restyle drops — is a reshape; and an earlier
axis-only restyle under a later reshaping one over the same text re-shapes
too, so the later declaration is the one that stands.

`spanPaint` and `spanStyle` resolve their selection as TEXT RANGES rather
than glyphs, because a restyle runs on the paragraph before there are
glyphs to point at: `sel::text` and `sel::regex` through weave's query
layer, `sel::word`, `sel::words`, `sel::sentence` and `sel::range` through
the paragraph's own structure, `sel::style` through the named runs the
content declared, and `sel::line` through the layout. Two consequences
follow. `Selector::take` and `Selector::drop` slice glyphs inside a unit,
which no text range can express — an `sel::each` selector restyles its
whole units and the slice warns once. And a `sel::line` restyle costs a
second layout pass and addresses the layout of the text BEFORE the restyle:
it does not chase its own result, so a `spanStyle` that moves the line
breaks leaves the selection where the first breaking put it.

**Layout options, fluently.** `Element::textAlign`, `Element::lineBreak`
(greedy or Knuth-Plass), `Element::hyphenation`, `Element::ellipsis`,
`Element::maxLines` and `Element::lastLine` set the general knobs of
`sigil::weave::ParagraphLayoutOptions` on any content form. The rest of
that struct — justification elasticity, Knuth-Plass tolerance, tab stops,
line-metric overrides — stays behind the paragraph overload, which takes
the whole options value. **On that overload the setters override FIELD BY
FIELD**, and only the fields actually set: everything a setter did not name
keeps the value that was passed in.

### Vertical CJK

`Element::writingMode` sets the passage running down the page.
`sigil::weave::WritingMode::kVerticalRL` is the CJK book layout: characters
top to bottom, columns advancing RIGHT TO LEFT from the node's right edge.
It is a field-masked override like every other layout setter, so it works on
plain text, on `rich()` spans, and on the paragraph overload — where a mode
nobody names leaves the paragraph's own mode standing.

```cpp
text(rich(mincho)
         .add(u8"平成")
         .add(u8"31", tateChuYoko)
         .add(u8"年、縦組みに対応した。"))
    .width(260).height(300)
    .writingMode(sigil::weave::WritingMode::kVerticalRL)
    .fx({.effect = fx::rise(24), .stagger = stagger(unit::Cluster)});
```

**BOTH AXES ARE MEASURES.** A horizontal passage reads its width as the
measure and grows down; a vertical one reads its HEIGHT as how far a column
runs before the next one starts, and grows LEFT. So a vertical leaf's
intrinsic size swaps: one column of type measures tall and one column pitch
wide, and giving the node no height gives it one endless column. It has no
baseline — the reading axis is y and a column's glyphs centre themselves
across the axis rather than standing on one — so `Align::Baseline` gets its
first character's own baseline, which lines a column's opening character up
with a horizontal neighbour's first line.

**Per character the orientation is UTR#50's**: ideographs stand upright and
take their `vert` forms, Latin lies on its side. A run that wants otherwise
says so in its own style — `sigil::weave::VerticalForm` is `kAuto`,
`kUpright`, `kRotated` or `kTateChuYoko` — set on a `rich()` run's
`sigil::weave::TextStyle` or through `spanStyle`. It is a SHAPING field, so
it re-shapes the words it covers and nothing else; there is no separate verb
because there is no separate concept. 縦中横 is the one to know: a short run
shaped horizontally and set upright across the column, which is how two-digit
numbers read in vertical prose.

**What else the face keeps for a column it hands over only when asked.**
Setting a run down the page applies the `vert` forms by itself; the wider
`vrt2` rotation set, punctuation recentred (`valt`) or fitted to its ink
(`vpal`, `vhal`), kana cut for a column (`vkna`) and vertical kerning
(`vkrn`) are named features, spelled as
`sigil::weave::Features::verticalRotatedForms` and its siblings and set on
`shaping.fontFeatures` like any other. They are part of shaping identity, so
naming one re-shapes the runs it covers — and they are NOT gated on the
writing direction, so a style carrying them and set along a line takes them
there too.

**The engine runs in columns.** `unit::Line` IS A COLUMN here, so a
`stagger(unit::Line)` beats column by column and `sel::line(0)` addresses
the rightmost one; `unit::Cluster` runs down a column in reading order.
`spanPaint`, `spanStyle`, `textAlign` (start is the top of the column),
`maxLines` (which clamps COLUMNS), `lastLine`, `lineBreak`, `textStroke`,
`variationDrive` and `feed()`'s text tier all work as they do across a line.
`mark()` anchors as it does anywhere — its rect is the union of the advance
boxes its selector addressed, and in a column those stack downward, so a
phrase's mark is a tall box standing in that phrase's column.
`Element::textFill` maps its unit square onto the COLUMN BLOCK rather than
onto a cap band — a column's glyphs centre across its axis instead of
standing on a baseline, so there is no cap band to hang a ramp on — which
means a gradient authored in [0,1]² crosses the type reading DOWN the page.

**Track deviations apply in the frame the layout placed the glyph in**, the
same rule a path baseline follows — and in a column the placed frame is the
glyph's own vertical pose. An UPRIGHT glyph is not turned, so its frame is
the canvas frame: `fx::rise` lifts it up the page. A ROTATED one is turned
to the column, so its frame is turned with it and a rise lifts it off its own
baseline, across the column. A glyph's pivot moves too: an upright glyph
turns and scales about the point on the COLUMN AXIS its pen reached, not
about a point half a column pitch to its right.

**What does not follow the type down the page.** `onPath` ignores
`writingMode` entirely — a path run's baseline is its own geometry and has
no columns to advance — and setting both warns once and keeps the path.
`flowAround` exclusions are cut out of horizontal line bands and have no
column spelling: they warn once and the columns run clean. An `ellipsis`
marker needs a straight horizontal final line to land on, so a clamped
vertical passage reports its overflow without drawing one. A decoration on
a span DOES follow the type down the page — an underline runs beside the
column on its right, an overline on its left, a strikethrough down the
column axis and a highlight across the whole column pitch — but it never
skips ink there, because ink intercepts are cut out of a horizontal band
window that a column's band is not. A BAND AND A TRACK ON ONE NODE DO NOT
COMPOSE, in either writing mode: a track draws its own glyphs in batched
buckets and a bucket carries glyphs alone, so the band is not drawn and
the node says so once. Split them — the passage that wears the band
stands still, and the one that moves wears none.

**Ruby and kenten are not library features**, deliberately. Each is a few
lines over the placed runs of a finished layout — read
`Composer::paragraphLayout` and draw beside what it reports — and the shapes
that annotation takes differ enough per passage that a verb would fit none of
them. The SigilWeave gallery's vertical scene shows both.

---

## The header map

Everything lives in `namespace sigil::compose` under
`include/sigilcompose/<feature>/`, one directory per feature target, and
the include spelling is the feature's: `<sigilcompose/core/Element.h>`,
`<sigilcompose/shape/Shapes.h>`. The public include root is `include/`
and nothing else — the internal headers beside each feature's sources are
not reachable from outside it. Each feature has an umbrella named after
it (`core/Core.h`, `shape/Shape.h`, `brush/Brush.h`, `paint/Paint.h`,
`typography/Typography.h`) over its public headers, and
`<sigilcompose/Compose.h>` at the root is the transitional umbrella over
the kernel — exactly `core/Core.h`. Each header stands on its own; include
the one a translation unit needs, from the feature whose target the
translation unit links.

**Kernel — `core/`.** A user who reads these headers has a complete and
sound model; nothing below them changes kernel semantics.

- `core/Motion.h` — the re-exports of SigilMotion's animation vocabulary
  (`Animatable`, `Transition`, `animate`, `bind`, `ease::`,
  `quantizeTime`), so authoring never has to name a second library.
- `core/Paint.h` — the paint values: `Fill`, `Corners`, `PaintContext`,
  `StampCache`, `UniformBlock`, `Effect`, and the colour spellings `hex`,
  `alpha`, `mul`, `mix`.
- `core/Text.h` — the text model: `Unit`, `Selector` and `sel::`,
  `TextEffect`, `Stagger`, `Track`, `Beat`, the mixed-text value `rich` /
  `RichText`, and `toU8`.
- `core/Shape.h` — the comparable seam values `Shape` (with
  `ShapeScheme`), `MotionPath`, `TextPath`, `Decoration` and its
  declared-volatility concepts, and `LayerStyle`.
- `core/Stroke.h` — the stroke grammar: `Spans` and `spans::`, `Profile`
  and `strand::`, `Across`, `Around`, `Formation`, `Shaper`, `StrandPath`,
  `Crossing`, `CrossingRule` and `crossing::`.
- `core/Mask.h` — the masking family: `Region`, `parts::`, `by::`, `Gate`,
  `Mask`.
- `core/Layout.h` — `Dim` and its literals, `Align`, `Justify`, `Echo`,
  `Cache`, `LayoutInput` / `LayoutScheme`, and the `ComponentProps` /
  `ComponentFn` concepts.
- `core/Element.h` — `Element` and its builders, the class alone.
- `core/Factories.h` — the functions that start one: `box`, `stack`,
  `positioned`, `text`, `image`, `custom`, `slot`, `layout`, `memo`.
- `core/Measure.h` — the one-shot verbs that take a tree without a live
  composer: `snapshot`, `measure`, `metrics`, `measureRun`, `runPens`.
- `core/Tiles.h` — `tiles::`, the slicing of one baked picture into a run
  of tile-sized rasters.
- `core/Derive.h` — `connector`, `rail`, `Anchor`, `band`, `bandPointAt`,
  and the `derive::` namespace that gathers the family.
- `core/Env.h` — the `env::` inherited-value channel, SigilCore's under
  the compose name.
- `core/Composer.h` — `Composer`.
- `core/Material.h` — the polymorphic paint value that supersedes a flat
  `Fill` — gradients, images, raw SkSL with live uniforms (float, float2,
  float4 and whole arrays, constant or live: a scalar binds an `Output`,
  an array binds a caller-owned `UniformBlock` whose `commit()` publishes
  an edit), SkSL as source compiled and cached by the library, blend
  stacks that compile to one shader, world-space anchoring — and the
  one-line gradient `Fill`s, `linearGradient` and `radialGradient`.
  `Effect::uniform` takes the same shapes on the post-processing seam.
- `core/Feed.h` — the streaming collection: a `feed::Ring` of rows,
  windowed to the newest `feed::Options::visible` and keyed by sequence
  id, so an append costs one mount and every surviving row keeps its
  cached picture; rows of text name their style in a
  `sigil::weave::StyleSet` (`feed::TextRow`, `feed::TextOptions`). Built
  purely by composing the kernel; the bordered strip several feeds sit on
  is the kit's `kit::plate` (`kit/Plate.h`), with `kit::tinted` building
  the one-face style set its rows name.
- `core/GpuImage.h` — `gpuimg::drawLattice` and `gpuimg::drawSpriteAtlas`,
  which are mandatory rather than convenient (see the traps).

The two time helpers a scene reaches for — `motion::ramp`, a delayed
eased `Transition` in float milliseconds, and `motion::phase`, a wrapping
`[0, 1)` over a period — are SigilMotion's, in
`<sigilmotion/Animation.h>`.

**Geometry — `shape/`.** `shape/Shapes.h` is the silhouette and curve
library, one include over four catalogs — every generator is a comparable
value, so a shaped node prunes like an unshaped one: `shape/Generators.h`
(the closed silhouettes: an SVG path, polygon, star, circle, annulus,
squircle, blob, arc, sector, parallelogram), `shape/Curves.h` (the
parametric curves in the unit frame: `parametric`, Lissajous,
harmonograph, rose, spiral, trochoid), `shape/Corners.h` (`rounded` over
any shape, `chamfered`, `notched`) and `shape/Edges.h` (`edges`,
`onEdges`, `inset`, `arrow`). `shape/Layouts.h` holds the placement
schemes for the `layout()` seam (`layouts::Radial`, `AlongPath`,
`ModularGrid`, `Diagonal`, `BaselineGrid`, `Scatter`). `shape/Routers.h`
holds the stock connector and rail routers (`routers::straight`,
`orthogonal`, `polyline`, `octilinear`, `orbit`).

**Marks — `brush/`.** `brush/Decorations.h` has the concrete primitives
that plug the `Decoration` seam — `PathFormat` (stroke formatting) and
`stroke`, its one-line spelling; `Shadow` / `shadow`, the soft drop
shadow; `Slice` (lattice image mapping); `ContourWalk` (walk the outline
and run a program at each sample); `Wash`; `Border`. The brush engine is
three headers: `brush/Layered.h`, the stroke stack (`StrokeLayer`,
`LayeredBrush`); `brush/GeometryOps.h`, the one mechanism door for
deviating an outline (`ops::`, `GeometryOp`); and `brush/Brushes.h`, the
brush kinds over them — `brush::solid`, the composites `brush::layers`
and `brush::weave`, and the archetypes `brush::Scatter`, `brush::Pattern`,
`brush::Ribbon`, `brush::Art`. The line vocabulary is three more:
`brush/Lines.h`, the cartography and diagram stroke (`lines::Line` —
parallel casings, terminal caps, ties, waves); `brush/Rails.h`, N-rail
strokes where every rail is its own line; and `brush/Hatches.h`, the
parallel, radial and concentric hatches. `kit/Strokes.h` and
`kit/Plate.h` ship with this tier because they are spelled in its types.

**Fills.** The paint vocabulary is SigilMaterial's, spelled as compose
values. `brush/LayerStyles.h` is the Photoshop route to rich surfaces:
bevels, sheens, inner shadows built from gradients and blurs rather than
shaders, and the gel and chrome bundles over the kit's colour tables.
`core/Sdf.h` gets shape, border, glow and soft shadow out of a single
shader pass. `core/Pattern.h` and `core/Patterns.h` bake tile recipes
once into repeating materials, plus stock generators. A material recipe
is a `Material` through `Material::recipe`, an effect through
`Effect::recipe`, and an output-stage view transform for
`Composer::setView` is SigilMaterial's colour transform, compiled only
when the build finds OpenColorIO.

**Type — `typography/`.** `typography/TextFx.h` supplies the stock preset
effects (`fx::rise`, `fx::slide`, `fx::pop`, `fx::spinIn`, `fx::typeOn`,
`fx::waveLoop`, `fx::scatter`, `fx::variableAxisSweep`, `fx::tint`) for
the kernel's `Element::fx` seam — and `marquee`, the seamless ticker
built from a clipped strip and a wrapping phase. The effects the runtime
evaluates by structure are declared with the kernel in `core/Text.h`:
`fx::scramble`, the `fx::keys` keyframe table, the `fx::pass` shader pass,
the `fx::seq`, `fx::mix` and `fx::hold` combinators, and the `fx::effect`
door. `typography/Type.h` is the compose-side spelling of a text style:
`type` builds a `sigil::weave::TextStyle` from a designated-init `Type`,
and `pickFace` resolves the first installed family of a fallback chain.
`kit/Legibility.h` ships with this tier.

**Leaves with their own targets.** `instances/Instances.h` renders
thousands of sprites as one leaf, with the pool on your side of the seam;
it is its own target, `SigilComposeInstances`, linked only by what stamps
with it, and the kit's `kit/Placers.h` (the `place::grid`, `place::ring`
and `place::repeat` pool fillers) ships with it. `web/Web.h` makes a live
Ultralight page a leaf; it is a header-only adapter and the library does
not link SigilScry, so include it only in targets that do.
`texture/Texture.h` is the door OUT of this library: a scene painted into
a surface and handed over as a SigilMaterial texture value, in its own
target `SigilComposeTexture` — see Boundaries.

**Testing — `testing/Checks.h`.** A separate target, `SigilComposeTesting`,
whose one header verifies generated geometry and reads back what was
drawn, in `namespace test` (GoogleTest owns `::testing`): `test::coverage`, `test::endpointDegrees`,
`test::rasterize`, `test::check`, `test::report`,
`test::failures`. The checks themselves — `test::check` and
`test::failures` — are SigilMeasure's, brought into `test`; only the
geometry readers and the feed `test::report` are this library's. Test
binaries link it, and so does the sketch library, so a sketch can report
its own checks; nothing that ships does, which is what keeps a
point-sampled coverage scan out of a paint loop.

**Kit — `kit/Kit.h`.** A tier above the library that adds no kernel state
and no new equality: `kit::Frame` and `kit::Grid` (figure-local polar and
unit coordinates) with `kit::disc` and `kit::centred` (a box about a
centre) and `kit::at` (a box pinned at absolute coordinates, for the
plate that has no layout at all), `kit::dotSprite` (the round stamp a
point sink draws each point with), `kit::ticks` and `kit::chords`
(division ladders as one path),
`kit::PixFont` (aliased bitmap-font bakes), `kit::Scrim` and the
halo/shade legibility helpers, the two instruments for text in motion —
`kit::trackMeter` (a cascade's schedule drawn, one cell per beat at its
rect, filled by its local time) and `kit::restGhost` (the same word
undeformed under the moving one) — and, shipped with the tiers whose
types they are spelled in, `kit/Strokes.h`'s shapers, profiles and span
compositions and `kit/Plate.h`'s bordered feed plate (Brush),
`kit/Legibility.h` (Typography) and `kit/Placers.h` (Instances). The kit
is a **separate CMake library** (`SigilComposeKit`) whose only include
path is compose's public headers, which is how the public/internal
boundary is proven rather than asserted. Note that `kit/Kit.h` does not
pull in the four headers shipped with other tiers; include them directly.

---

## The declared-volatility contract

The library caches provably-static subtrees on its own. A `Cache::Auto`
subtree with nothing volatile in it records into a picture; a node that has
been expensive for several consecutive frames may be re-baked into a raster
image and blitted thereafter. None of that is safe unless "static" is
*true*, and nothing in the library can introspect a type-erased value to
find out.

**So anything that changes without a re-describe must say so.** Concretely:

- A decoration whose paint moves — a bound dash phase, a walk keyed to
  elapsed time — declares `bool isAnimated() const`. The seam reads it off
  the value at construction; a scheme that stays silent is treated as
  static and its node's picture will be replayed forever.
- A `custom()` paint program that reads the clock (or anything else the
  library cannot see) must declare `.cache(Cache::None)`. It is the
  immediate-mode floor and it costs a repaint per frame, which is the
  point.
- A `Material` that reads `uTime` or carries a uniform bound to an
  `Output` is live by construction and declares itself; so is an `Effect`
  with a bound uniform or a live child. Tier inheritance is real: a live
  child makes the parent effect live, so no cache can freeze the
  parameter.
- A decoration that paints beyond the node's box declares `bleed()`, and
  one that needs to say how wide the *mark* is declares `reach()`. These
  are different numbers — an inner-aligned stroke bleeds zero while
  painting a mark several pixels wide. Over-reporting is safe;
  under-reporting silently truncates cached pictures.
- A decoration that reads another element's resolved path declares
  `borrows()`, so the element can register the keys without looking inside
  the value.

The counterpart obligation is equality. Anything read live must
participate in the reconciler's structural comparison, or a pruned node
reads a stale value forever. The conservative fallback is built in:
anything holding an incomparable callable compares *unequal* and never
prunes.

---

## Traps

### Silent no-ops, the largest class

Several correct behaviours produce nothing, with no diagnostic, and look
exactly like a layout bug.

- **An unknown key resolves to nothing, everywhere in the derive family.**
  `flowAround("typo")`, `spans::fit("typo")`, `around("typo")`, a
  `connector` to a node not in the tree, a `strand::from` on a missing key
  — every one draws nothing and says nothing. Check your keys first. (A
  `rich().slot()` name is the one that is LOUD, once: it names a mount point
  the author typed, not a geometry source.)
- **Hit testing returns any keyed node whose box contains the point,
  painted or not.** A keyed full-bleed layout shell with no fill therefore
  swallows every hit in the frame, and the failure is total and silent.
  The opt-out is `Element::hitTestable(false)`, which excludes the node's
  own box while still testing its children.
- **Skia's native lattice and atlas draws are not implemented on
  Graphite** in this Skia — they draw nothing. Worse, one recorded on a
  raster canvas still vanishes when the recording replays on Graphite, so
  a raster test cannot see it. Use `gpuimg::drawLattice` and
  `gpuimg::drawSpriteAtlas`, which decompose on every backend and never
  emit the native op. This is not an optimisation layer; it is the only
  correct path.
- **A `custom()` leaf sizes like an empty box.** It is literally a box with
  one background program, so it has no intrinsic size: dropped into an
  `absolute().inset(0)` parent it measures zero on the main axis and the
  program runs against a zero-height context. Give it dimensions, or make
  it `absolute().inset(0)` itself.
- **`.key()` on a `slot()` renames the mount.** A slot's name *is* its key,
  so `slot("hud").key("panel")` produces a slot called `"panel"` and
  `renderSlot("hud")` then finds nothing. It warns once.

### Lifetime

Every live binding is a **non-owning raw pointer** to an `Output` the
caller owns; the composer holds its `FontContext` and its `Ticker` by
reference, and both must outlive it. A recreated `Output` at a new address
does more than dangle: bindings compare by *identity*, so the new address
also breaks prune equality and re-patches the node on every describe. Hold
your outputs where you hold your model.

### Pruning

The raw-callable escape hatches — a `Shape` built from a lambda, an
unkeyed `custom()` program, a bare `PaintProgram` decoration — can never
compare equal to a separately constructed one, so their nodes re-patch on
every describe. The fix is to hold the value rather than re-minting it,
or to wrap the node in `memo()`. Two spellings avoid the problem outright:
`custom(key, program)` makes the key the program's identity, and every
`shapes::` generator is a comparable value.

This matters more than it sounds. An inherited value carried through
`env::` that holds a `std::function` is incomparable, and that turns every
`memo` below it into a permanent miss. Materialise derived values *into*
the type: run the function, store the result.

### Ordering

Decoration stacking is a contract, not a hint. `background()` paints
*beneath the fill*, so an opaque fill covers it completely — a bevel added
with `background()` on a filled node draws underneath its own surface and
looks like nothing happened. The slot between the fill and the content is
`overlay()`; above the children is `foreground()`.

Within `stroke()`, unqualified whole-boundary strokes paint first and
span-qualified passes paint over them, each group in declaration order.
Interleaving the two groups by call order is not expressible; make the
whole-boundary one a span pass (`spans::every(1)`) so both are in one list.

Span-qualified passes also *claim* the runs they resolve to, and two claims
that overlap are reported out loud. Stacked masks intersect where their
selections overlap — both gates must pass — and union is spelled inside one
gate value by combining spans with `|`, never across masks.

One coordinate convention, stated once and obeyed everywhere: **positive
`across` is to the LEFT of travel**, which in screen space (y down) is
outside a clockwise path. `bandPointAt`, `Profile::across`,
`strand::offset`, `TextPath::offset` and the `lines::` family all mean the
same side. Relatedly, **fraction 0 on a boundary is the bottom-left
corner**, running up the left edge — so `spans::upTo(0.25f)` on a square is
the left edge, not the top one.

---

## Boundaries

The library links `SigilCoreReconcile`, `SigilCoreCache`,
`SigilGeometryPath`, `SigilImage`, `SigilMotion`, `SigilWeave` and Skia
publicly, and Yoga privately. `SigilCoreReconcile` is the reconciler: the
keyed and positional match, the memo, the identity prune, the `env::`
channel and the animation lane operations are its, and `Composer` is its
host — the description comparators, Yoga, text and paint stay here.
`SigilCoreCache` is the caching kernel, and `Composer` is its host too:
the three-valued cache policy (`cachePolicy` maps this library's
five-valued `Cache` onto it, keeping the TIER — picture, texture,
group — on this side), the fold that turns one node's declarations and
its children's verdicts into what a subtree promises, the stability
release that proves a node declaring volatility is holding still, and the
three-way bake decision are its. What every term MEANS is compose's: which
Skia paint moves pixels off the describe clock, which of its lanes a value
memo can compare, what a recording is and when it may be replayed.
OpenColorIO is optional and gates `paint/Ocio.h` alone.
`SigilGeometryPath` supplies the contours, polylines, poses and seeded
noise that every outline walker here reads through, and compose adds no
path geometry of its own. `travel()`'s motion path is the worked example:
the curve is measured into that library's contours once per shape and
size, and each frame's position is one pose read along them, walked as a
single arc-length coordinate. What stays here is the
POLICY the verb states — the fraction wraps on a closed curve and clamps
on an open one, the tangent angle comes from a look-ahead chord, and the
path outranks the translate lanes.

`SigilComposeTexture` is the one feature that owns a SURFACE, and it is
the exception the bullet below states. `compose::TextureScene` keeps a
composer and the surface it paints into, and hands the picture over as a
SigilMaterial texture value: a consumer that samples an image samples
that one with no knowledge that a composer made it, and nothing above has
to learn what an `Element` is. `compose::texture` is the one-shot form,
for a picture described once. The version the value carries counts
PAINTS, not describes — a frame whose reconcile moved nothing leaves the
value equal to the frame before's, which is what lets a consumer prune on
it. The surface is a raster one by default and a texture on a GPU device
when a host hands the scene one, so a renderer standing on that same
device binds the pixels where they were painted rather than copying them.
The arrow points one way: this feature links SigilSkia and SigilMaterial's
texture feature, and nothing that samples the value links compose.

Deliberately *not* linked: SigilScry (the web leaf is a header-only
adapter, exercised by its own test target), EnTT (the instancing header
keeps the registry on your side), the mesh-and-material `SigilGeometry`
above the path leaf, Diligent, and Qt — Qt identifiers are banned
outright in exported headers.

What it refuses to be:

- **No markup, parser or external DSL.** Markup can only name
  pre-registered values; the vocabulary here is C++ values and callables.
  A serialization schema can be a *producer* of element values, never the
  API.
- **No imperative node mutation.** Describe or bind, and nothing else.
- **No timeline object.** Multi-beat choreography is windowed bindings
  over one phase output (`bind(&phase).window(lo, hi)`).
- **No surface, loop or thread ownership — outside `texture/`.** The
  composer is a guest in someone else's canvas, and a host that wants
  many surfaces makes many composers. `compose::TextureScene` is the one
  place a surface is owned, because a picture another library samples has
  to live somewhere and the alternative is every such consumer writing
  the same three lines.
- **No depth and no perspective in the model.** There is no z, no
  `rotateX`, no projection. A camera, if you want one, is the host's
  matrix on the canvas — a recording is matrix-independent by
  construction, so a moving camera invalidates nothing the library holds.
  The places that pin pixels to a device rect refuse a perspective matrix
  explicitly.
- **Compositing happens in encoded sRGB, with no linear stage.** Every
  surface compose paints into is `N32Premul` with no colour space
  attached, so the `SkColor4f` you write is the display-encoded number
  that lands in the byte and a shader's channels are those same numbers.
  Any weighting of colour channels inside the library uses coefficients
  defined on encoded values. `Composer::declareInputSpace` lets you state
  what you believe your values are; a mismatched declaration warns once
  and performs **no** conversion, because a colour-managed surface would
  be a breaking change rather than a setting.

---

## Build and test

The library is one feature target per directory, and a consumer links the
tier it draws with: `SigilComposeCore` (`core/` — the kernel: elements,
layout, paint, transitions, text and the feed, as the host of
SigilCore's reconciler),
`SigilComposeShape` (`shape/` — silhouettes, layouts, routers),
`SigilComposeTypography` (`typography/` — the text engine behind dressed
type, with the type styles and the text-fx presets), `SigilComposeBrush`
(`brush/` — decorations, lines, brushes, the stroke grammar's engine and
the mask gates, with `kit/Strokes.h` and `kit/Plate.h`), `SigilComposePaint`
(`paint/` — patterns, SDF materials, layer styles, OCIO),
`SigilComposeInstances` (`instances/` — the instanced sprite leaf and the
kit's placers, over Core), `SigilComposeTexture` (`texture/` — a scene
painted into a surface and handed out as a texture value),
`SigilComposeWeb` (`web/` — header-only, present only with SigilScry),
`SigilComposeTesting` (`testing/`) and `SigilComposeKit` (`kit/`). Each directory holds the target's sources,
its internal headers, its `test/` and its `bench/`; the public headers
sit under `include/sigilcompose/<feature>/`. Every consumer in this
repository — SigilSketch, the benches, the demos, the tests and
`geometry_demo` — links the feature targets it draws with by name, so a dependency on a tier is a stated fact.
`SigilCompose` remains as the whole-library name for a consumer outside
this tree, the way `SigilWeave`, `SigilMotion` and `SigilGeometry` each
keep one: it is Paint (which reaches Brush, Shape and Core) plus
Typography, never the instanced leaf or the web leaf, and nothing here
links it. From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Registered tests, one binary per feature target so that each links only
the target it exercises and a test reaching past its tier fails to link:
`compose_core_test` (the kernel — elements, the reconciler, layout, paint,
transitions, text, the feed, masks and the field walks; links
`SigilComposeCore` alone), `compose_shape_test` (silhouettes, layouts,
routers, rails, travel), `compose_text_test` (text data, the text pass,
vertical writing, motion along paths, the text-fx presets, rich spans),
`compose_brush_test` (decorations, lines, brushes, the stroke grammar,
the kit's stroke presets), `compose_paint_test` (patterns, SDF materials,
layer styles, colour management), `compose_instances_test` (the pool,
the atlas, the stamp, the pick and the placers), `compose_kit_test` and
`compose_studio_test` (the kit, and the queries, the studio and the
instruments over it), `compose_spike_test` (the Yoga+SigilWeave
measurement contract, with `core/`), and the library's own:
`compose_docs_test` (the engine walkthroughs and the generated README
probes) and `compose_api_doc_probes_self_test`,
plus `compose_gpu_test` (Apple only, needs the Graphite plumbing) and
`compose_web_test` (needs the Ultralight SDK). Each binary's translation
units share `test/support/Host.h` — the composer-in-a-raster-surface
harness — through a support header of their own that includes only what
they use. The benchmarks and `compose_demo` are executables, not tests.
There is one benchmark binary per tier — `compose_core_bench`,
`compose_shape_bench`, `compose_brush_bench`, `compose_paint_bench`,
`compose_text_bench` — each in its feature's `bench/` over the shared
`bench/BenchSupport.h`, linking only the library it measures, all built
by the `benches` target and run by `scripts/bench_ledger.py`; anything
resembling a performance claim belongs to them and to the plate ledger,
never to prose.

**Looking at any of it** goes through SigilSketch, which is where every
renderable thing in this repository lives: one file per scene, one
registry, one application (Sketchbook) and one headless renderer.
`src/sketch/README.md` is the canon for it — how a sketch is written and
registered, how the live host reloads one, and how the plates a
byte-identity sweep hashes are made. Nothing here hosts a catalogue of
its own.

### The generated doc-probe translation unit

`compose_docs_test` builds a C++ file that does not exist in the source tree.
`test/docs/api_doc_probes.py` reads this document, extracts every qualified
name an author could copy out of it — from fenced code blocks **and** from
inline `code` spans, because the prose carries as many names as the
examples do — and emits probes that only compile if the headers still spell
those names that way. A member is probed through a `requires` expression, a
namespace-scope entity through a using-declaration, and a designated
initialiser through the initialiser itself, which is a stricter question
than whether the name resolves.

The consequence is the point: a name written here that drifts out from
under the prose is a build break, not a confident wrong answer. Names the
document mentions on purpose without their existing — a worked example's
own host type, a spelling recorded because it was removed — go in the
generator's exclusion table with a reason. A documented name that resolves
to nothing and is not excluded fails the generator, so the guard cannot go
quiet.
