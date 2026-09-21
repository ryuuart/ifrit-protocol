# Text fx

A chapter of [TYPOGRAPHY.md](../TYPOGRAPHY.md), the type chapter of
[SigilCompose](../README.md).

Motion inside a text leaf is a list of **tracks**. One `Track` is five
values — *which* glyphs (`weave::Selector`), *what* deviation from rest
(`TextEffect`), *how* the beats spread (`motion::Spread`), what a unit IS
(`Track::unit`), and the master `Animatable<float>` progress that drives
it. The spread is SigilMotion's and says nothing about text; `unit` is the
whole of what makes it a cascade over glyphs rather than over a set's
children or a feed's rows. `Text::fx` appends one;
several compose per glyph, with `GlyphModifier` offsets and rotations adding
and scale and alpha multiplying. The seam is three headers:

- `typography/TextEffect.h` — the effect as a VALUE. `GlyphInfo` is what
  a body is handed, `GlyphModifier` what it returns, `GlyphModifierFunction` the callable
  those two make, and `TextEffect` the comparable value one is wrapped
  in.
- `typography/TextFx.h` — the `fx::` catalogue: the effects the runtime
  evaluates by STRUCTURE rather than by calling a body, and
  `kNominalSizePx`, the display size a preset's reach is declared
  against.
- `typography/Track.h` — the cascade. `Track` is the five values above,
  and `Beats` is which list its beats are numbered against.
- `kit/Kinetic.h` — the stock effects an example reaches for: `rise`,
  `waveLoop` and the rest are values over the seam, and so the kit's.

```cpp
text(u8"ONE LINE, TWO MOVES", display)
    .fx({.effect = fx::rise(20), .unit = weave::Unit::Word})
    .fx({.where = weave::selectors::text(u8"TWO"),
         .effect = fx::waveLoop(),
         .progress = &phase});
```

**Units.** `weave::Unit` is the granularity a selector slices and a cascade
beats over: `weave::Unit::Glyph`, `weave::Unit::Cluster`,
`weave::Unit::Word`, `weave::Unit::Line`, `weave::Unit::Sentence`,
`weave::Unit::Selection`. `weave::Unit::Cluster` is the default, and it is
the one that keeps text correct — a base letter and its combining marks
are one unit and never separate under a stagger. `weave::Unit::Selection`
is the odd one: not a size the text is divided into but the extent the
selector named, one unit per extent it addressed — and two where two of
them touch — which is what puts one reading over a compound the breaker
is free to divide.

**Selectors.** `weave::selectors::word`, `weave::selectors::words`, `weave::selectors::line`,
`weave::selectors::range`, `weave::selectors::text` and
`weave::selectors::regex` name a position in the text; `weave::selectors::each` slices
every unit of one granularity the same way, with `weave::Selector::take` and
`weave::Selector::drop` partitioning each unit exactly. Combine with `|`,
`&` and `!`. A default-constructed `weave::Selector` addresses everything.
Selection is resolved once per (content, layout, selector) and cached on the
element; a pattern that does not compile selects nothing and warns once.

`selectors::style` is the odd one out and addresses the TREATMENT rather than a
position: every run a `weave::rich()` value added under a style name
(`weave::RichText::add` with a name resolved through a
`sigil::weave::StyleSheet`).

```cpp
text(weave::rich(base).styles(set.types())
         .add(u8"gusting ").add(u8"soon", "term").add(u8", then rain"))
    .fx({.where = !selectors::style("term"), .effect = fx::variableAxis("GRAD", 900)});
```

A glossary set in one registered style stays addressable when the copy
changes, where naming the literal words means editing the selector every
time an author edits a sentence. It resolves through the run's TEXT, so
re-registering the name against a different style — or a `spanPaint` or
`spanStyle` cutting across the run — leaves the same runs selected. Only a
named `weave::rich()` run carries a name: plain text, a run given a style
directly, and the paragraph overload have none, so there it selects nothing
and warns once per name, as does a name no run was written with.

**Cascades.** `motion::Spread` keeps the GSAP model — `eachMs` or
`amountMs`, `durationMs`, and a `motion::Spread::From` origin: `Start`,
`Center`, `End`, a seeded `Random` and a two-ended `Edges`. `Random` deals
a scrambled EVEN ladder — every unit takes a distinct rank, so no two units
open together — and it is deterministic: the ranking hash is keyed on the
unit count and the seed, so the same text scatters the same way on every
frame and after every relayout. At the default seed of 0 the key is the
count alone, which makes two same-count cascades scatter identically; give
each field its own nonzero seed for independent scatters.
`motion::Spread::distribution` shapes the start times across the cascade,
and `motion::Spread::then` nests a second cascade inside every beat of the
first — `Track::innerUnit` says what a unit is at that second level.

**Irregular timing.** `motion::Spread::cues` replaces the even spread with
a TABLE — one start time per unit, in ms — which is what caption, lyric
and lip-sync timing actually is:

```cpp
text(lyric).fx({.effect = fx::rise(12),
                .stagger = motion::Spread{.durationMs = 180}
                               .cues({0, 340, 720, 1180}),
                .unit = weave::Unit::Word});
```

It answers the spread itself, so it goes anywhere one goes and compares
like one. A table says only *when unit k starts*; `durationMs` and `then`
are untouched by it, while `eachMs`, `amountMs`, `from` and `distribution`
have nothing left to say and are ignored. A unit past the end of
`motion::Spread::cueMs` starts at the last entry (the tail piles, visibly, rather than being given
times nobody wrote), entries past the last unit go unread, and either
mismatch warns once.

**Which list the beats are numbered against.** `Track::beatsOver` takes a
`Beats`: `beats::Selection` — the default, numbering only the units the track's own
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

**Solving a style backwards from a size the drawing states.** A reference
quotes how tall a capital stands, never a font size: `atCapHeight(style,
capPx, fonts)` asks the FACE for its cap height and scales to it, which
is the honest form of the `capPx / 0.72` written wherever this is done by
hand — that ratio is one face's, and on another it puts the lettering out
by whatever the two disagree by. `fitRun(utf8, style, widthPx, fonts,
fit)` solves the other axis. A run's width is AFFINE in its size, because
the ink scales and the tracking does not (tracking is px), so it takes
two measurements to identify the line and reads the size off it; a fit
written as one division assumes the line passes through the origin and
overshoots by exactly the tracking. `RunFit` is the ladder it walks down
— the size first, and the horizontal condense only over what the size
floor left. Neither floor is a promise to fit: a run that cannot reach
the width comes back at them, over-wide, rather than at a size nothing
could read.

**The whole span.** A beat says when it *opens*; `Composer::cascadeSpanMs`
says when the whole schedule is *over* — the ms of virtual time the track's
master progress [0,1] maps onto: `durationMs + eachMs·(N−1)` for the flat
even ladder, `durationMs + amountMs` in amount mode, the compounded extent
under `motion::Spread::then`, and the latest time any unit reads plus `durationMs`
under a cue table. It is the number a progress duration must equal for a
cascade to run at its authored ms — a table's times are absolute only when
the window driving the track spans exactly the span — and the number
anything sequenced *after* the cascade offsets from. It is computed by the
same resolved cascade the glyphs and `beatsOf` read, so the three cannot
disagree; an unknown key or track index resolves to 0, silently.
`Track::spanMs` is the same number at *declare* time, computed from unit
counts alone for the site that needs it before any node exists — above all
the progress transition written right next to the stagger:

```cpp
const motion::Spread cascade{.eachMs = 28, .durationMs = 480};
const float span = cascade.spanMs(13);  // 480 + 28·12, before any layout
// Drive the track's progress over exactly `span` ms and the last glyph
// lands as the master arrives at 1. After a draw,
// composer.cascadeSpanMs("title", 0) reads the same number off the
// mounted track — with the unit count the laid-out text supplies.
```

For a nested cascade the second argument is how many inner units one beat
holds (the widest beat's count, where they vary), and an amount-mode span
is the same for every count past one, because the amount *is* the spread.

**The looping cascade.** `motion::Spread::loopMs` makes the schedule wrap: above 0,
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
motion::Spread cascade = motion::Spread{}.cues(columnStartsMs);
cascade.then({.eachMs = 80, .durationMs = 1400});
cascade.loopMs = 5000;  // every column re-drops on its own cue, forever
text(field, rain).fx({.effect = streak,
                      .stagger = cascade,
                      .unit = weave::Unit::Line,
                      .innerUnit = weave::Unit::Cluster,
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
own entrance. `Composer::cascadeSpanMs` and `Track::spanMs` answer the
**period** — still the ms the master maps onto, and the number a driver's
wrap must span for the schedule to run at its authored ms. One loop governs
the whole cascade, read off the outer spec under `motion::Spread::then` as
`Track::beatsOver` is; `Beat::localT` reports the wrapped local time (the
same number the effect is handed) and no cycle index rides beside it — the
master is a phase mod 1, so cycle identity lives with whoever steps the
phase. Driving that phase is also what keeps the element live: a looping
cascade at a *constant* master is one still frame of its cycle, exactly as
a wave at one phase is, so permanent volatility is declared by the wrapping
binding, never by the field, and `loopMs = 0` — the default — is the
one-shot cascade.

**Marking the type.** `Text::textAttach` anchors a child to the rect a
*selector
resolves — a caret, a callout, a tick, a rule standing at a word's edge:

```cpp
text(line, style)
    .textAttach(weave::selectors::word(3), box().left(0).top(pct(100))
                             .width(pct(100)).height(2).fill(ink));
```

The child's box is that rect, and its own placement longhand is read
*inside* it, exactly as a `positioned()` child reads it against its parent —
so a mark with no dims at all simply is the unit's rect, and one with them
is free to hang outside it. That is the difference from `weave::RichText::slot`,
which reserves space *in the flow*: the line breaks around a slot and the
type after it starts further along, where a mark is placed on a line laid
out as though it were not there. A selector resolving several units gives
one rect, the union of all of them; one resolving nothing places nothing and
warns once. The rect is the **rest** rect — where the layout put those
glyphs, not where a track has thrown them this frame — so a mark follows a
reflow and stands still under a cascade; read `Composer::beatsOf` and drive
the mark's own transform for one that must ride the motion. On a path run
(`textOnPath`) the rect is on the curve, at the run's *resting* placement — a
run driven along its baseline is a paint-time deviation like any track's.
A mark needs no
`reach`, being a child: the recording cull already grows by the union of a
node's children.

**The rest pose is a description.** `Text::atRest` is this leaf as a
second element that can stand beside it: the same content, style,
measure and layout, carrying nothing that deviates or restyles a glyph
at paint time — no tracks, no span restyles, none of the leaf's marks or
slot mounts — keyed `-rest` after the original, its ink the caller's.
`kit::restGhost` is that copy under the moving one, set in one colour.

**Effects are comparable values**, which is what lets text carrying tracks
prune like any other static leaf. A preset compares by its name and its
parameters; an ad-hoc body goes through `fx::effect`, which takes the key
its author gives it — two different bodies under one key compare equal and
one of them silently never draws. The one declaration an ad-hoc body
carries, `TextEffect::displacing`, joins those parameters rather than
sitting beside them, so two bodies under one key that disagree about
placement do not prune onto each other. `fx::sequence` remaps local time so each
phase sees a renormalised 0→1 (`TextEffect::until` sets the joint,
`Phase::crossfade` lerps across it), and `fx::mix` evaluates several effects at
one time and composes them by the same algebra stacked tracks use.

Both are built through `TextEffect::composite`, which is also the door for
a combinator of your own: the operands RIDE the value, so the result
compares by structure — a `fx::sequence` of equal phases equals another built
the same way — rather than by the closure that evaluates it. It is also
where whether the result DISPLACES is derived rather than restated: a
composite moves its glyphs when any operand it may evaluate does, so a
sequence whose second phase lifts is displacing from the moment it is
built, and nobody has to remember to say so.

**Keyframe tables.** Every published web or motion reference is a list of
(position, value) entries, and `fx::keys` is that list as an effect. A
`fx::Key` is a moment in local time, a `GlyphModifier` at it, and optionally a
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
Interpolation is componentwise through the same arithmetic a `fx::sequence`
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

**What an effect is handed.** A body is `(glyph, local t, stream) →
GlyphModifier`, and `GlyphModifierFunction` is that callable — the seam never holds a
bare one, because a bare function cannot be compared, so it is wrapped in
a named `TextEffect` before anything can hold it.

The first argument is the glyph's own facts, and every index in it is a
fact about this glyph's place in THIS layout of THIS text, stable across
relayouts while the text is unchanged. Where it sits and how big it is:
`GlyphInfo::rest` is the pen position the layout gave it,
`GlyphInfo::advance` its advance width, and `GlyphInfo::fontSize` the
size an em-relative deviation is written against — which is how one
preset reads the same at a caption and at a headline. Where it sits in
the text: `GlyphInfo::index` and `GlyphInfo::count` in the paragraph,
`GlyphInfo::glyphInWord` and `GlyphInfo::wordGlyphCount` inside its word,
then `GlyphInfo::wordIndex`, `GlyphInfo::lineIndex` and
`GlyphInfo::sentenceIndex` for the containers it belongs to, with
`GlyphInfo::cluster` — a base and its combining marks share one value —
and `GlyphInfo::textIndex`, that cluster as an offset into the text. That
list is what lets a body say *the third letter of its word* or
*everything on line two* without the author counting glyphs by hand.
`GlyphInfo::unitIndex` and `GlyphInfo::unitCount` are the TRACK's own
numbering — which beat this glyph belongs to, in the list `Track::beatsOver`
chose — so a per-word track sees word ordinals there.

`GlyphInfo::styleIndex` is the one to read and never to address by: span
restyles cut and merge the style list, so a `spanPaint` anywhere ahead of
this glyph renumbers it, and the two resolvers could not be made to agree
on what a given index names. The handle on a treatment is the NAME the run
was written under, which `selectors::style` addresses.

**Effects get a `core::noise::Mix64Stream`**, seeded from the glyph's
identity, so a scatter is the same scatter on every frame and after every
relayout — which is what lets it settle and cache instead of jittering
forever.

**A shader per letter is one pass.** `fx::pass` makes a track's effect a
PASS rather than a per-glyph deviation: the runtime renders the units the
track addresses into a layer and runs the material ONCE over it, handing
the track's own schedule in as uniform data — `uContent` (the layer),
`uUnitRect[]` (each unit's box, node-local px) and `uUnitPhase[]` (each
unit's cascade-local 0→1, then a stable per-unit seed) — so per-letter
treatment is data rather than scene structure, and the cost is one draw
plus one pass whatever the unit count is:

```cpp
// emberDissolve is a SigilMaterial recipe over the parameter struct Burn,
// carrying the pass body as its SkSL.
auto burn = material::skia::Paint::recipe(
    sigil::material::Material(emberDissolve, Burn{ink}));
text(u8"EMBER DECODE", display)
    .fx({.effect = fx::pass(burn), .stagger = {.eachMs = 260}});
```

The paint must be RECIPE-BACKED — `material::skia::Paint::recipe` over a recipe
carrying an SkSL body — because the unit count is baked into the compiled
shader: a runtime effect's array size is fixed at compile and SkSL has no
uniform-bounded loop, so the runtime holds a specialization of that recipe
per distinct count, its body the declarations above plus `const int
kUnitCount = N` ahead of the author's. Write the body against those names
and do not declare them, and declare every uniform of your own as a parameters
field rather than in the body's text; any other material warns once and
the track draws its glyphs at rest. `main(xy)` runs in the node's own px, the layer is sampled at the
device's resolution (a 2x host stays sharp with no supersampled bake), and
the pass is BOUNDED: it paints the node's box grown by the track's `reach`
and nothing outside it, unlike an `Element::filter` shader pass. The
per-unit rects and times are resolved from the SAME cascade
`Composer::beatsOf` reports, so a pass, a mark and the glyphs cannot
disagree about the schedule.

`TextEffect::passMaterial` is what the runtime dispatches on: the pass
material for a pass effect, null for every per-glyph one. That is the
whole of the distinction — a pass is not a kind of track, it is an effect
carrying a material instead of a body.

**A pass can declare where it rests.** `fx::pass(m).restsAt(0)`,
`.restsAt(1)` and `.restsAt(0, 1)` promise the SkSL is an EXACT
pass-through at those unit phases. When every addressed unit's resolved
local time sits on a declared phase the runtime skips the layer and the
shader and draws the glyphs directly — so a settled pass on a node that
repaints for unrelated reasons (an orbiting `textOnPath` ring) stops paying
for a shader that changes nothing. The promise is unverifiable, in the
family of `reach` and `bleed()`: declare a phase where the shader is not
a pass-through and the picture pops at the seam, with no diagnostic. The
test is exact — a one-shot cascade clamps a unit to exactly 0 before its
beat and exactly 1 after. A looping cascade touches 0 only at the instant
a beat re-opens, so `restsAt(0)` effectively never engages there
(correctly — the cycle is always mid-flight somewhere), while units rest
at exactly 1 between beats, so `restsAt(1)` engages whenever no beat is
mid-cycle. Undeclared, a pass always runs — `TextEffect::restPhases` reads
back what was declared, and is empty both for a pass that declared nothing
and for every per-glyph effect, which have no such promise to make. The
declaration rides the effect's parameters, so two passes promising different
phases do not prune onto each other.

Order against everything else: deviation tracks apply FIRST, and the pass
reads the deviated pixels — a pass is post-processing, and pixels are what
it processes. A glyph a pass addresses draws only inside that pass's
layer, never directly as well; several pass tracks run in declaration
order, each over its own selection's layer, and a glyph two passes address
renders in both. A path baseline and a vertical column place glyphs before
any of this, so a pass rides both. A pass is a whole-track statement:
inside `fx::sequence`, `fx::mix` and `fx::hold` its material is not consulted —
sequence a pass by driving its progress, and gate its onset in its own
SkSL, which holds the whole schedule.

**Colour as a cascade.** `fx::tint(from, to)` is the colour reveal — a
karaoke wipe, a highlight sweeping a word — and it carries one inversion
worth stating once. `GlyphModifier::colorMultiplier` MULTIPLIES, and a multiplier only
takes a colour toward black, so **the element is set in `to` and the effect
multiplies down toward `from`**. The arguments still read in time order and
the division is done inside: `fx::tint(pale, sung)` on a line set in `sung`
wipes pale to sung, while setting the line in `pale` draws pale throughout
with no diagnostic. Multiplying is also what lets it tint a gradient-filled
line without knowing what fills it, and why a destination channel of zero
cannot be departed from.

The way *up* is the other two colour terms. `GlyphModifier::colorAdd` is the
**hard flash**: added to whatever the style paints — after the multiply,
clamped at the draw — it brightens where a multiplier can only darken, and
it *adds across tracks*, the sum clamping once, so two half flashes make one
full one. `GlyphModifier::colorScreen` is the **phosphor glow that never clips**:
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

**What a `GlyphModifier` can say.** Beyond `dx`, `dy`, `scale`, `rotateDeg` and
`alpha`: `colorMultiplier` multiplies every pass the glyph's style draws (a flat
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
last-one-wins — a `fx::sequence` crossfade cuts them at the middle of its window
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
`fx::variableAxis` holds a coordinate and `fx::variableAxisSweep` sweeps between
two across local progress
and `fx::scramble` is the decoding-text preset built on the substitution:
each glyph churns through a charset and resolves to the true letter by
`t = 1`, seeded per glyph so it is the same churn on every frame.

`Text::variationDrive` is sugar over a whole-text `axis` track, so a
driven axis composes with entrances and loops instead of being a second
text path they would hide.

**Snapping, and `Track::continuous`.** Rotation, alpha, the colour terms
(`colorMultiplier`, `colorAdd`, `colorScreen`) and the axis coordinate are
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
box it may throw a glyph — or takes the number its effect declares.
`Track::reachPx` is that resolution, the track's own number where it
states one and its effect's otherwise, and it is what the recording cull
actually grows by, on the same over-reporting-is-safe contract a
decoration's `bleed()` carries. Under-report and cached output is
truncated with no diagnostic.

A PRESET cannot know the size it will be drawn at, so the reach it
declares is measured against `fx::kNominalSizePx` — the display size a
glyph grown about its own centre is read at, and the size a keyframe
table's growths and leans are read against too. Drawn smaller than that,
a preset reserves more than it needs, which costs nothing; drawn larger,
it wants a `Track::reach` of its own.

`Text::textFill` and `Text::textStroke` combine with tracks and with a
path baseline alike: a letter in flight, and a letter on a curve, are painted
with the same glyph paint a resting one is. A layer style's echo, stated
through `Element::layerStyle`, skips fx text by contract.
