# SigilCompose typography

How a passage of type is written, dressed and read back — the chapter
`README.md` sends you to for anything more than `text(utf8, style)`.
Every mechanism here rests on SigilWeave, whose own `FEATURES.md` carries
the control-by-control table and names the verb on this side that reaches
each control.

- [Text fx](#text-fx) — the multi-track per-glyph seam, its selectors, cascades and presets
- [Text on a path](#text-on-a-path)
- [Mixed text](#mixed-text) — `rich()`, span restyling, and the layout setters
- [A passage whose input moves](#a-passage-whose-input-moves) — `live`, the budget, and what a frame reports
- [Paragraphs, frames and stories](#paragraphs-frames-and-stories)
- [Beside the text](#beside-the-text) — `Composer::units`, annotations, and the kit over them
- [Vertical CJK](#vertical-cjk)

What a decoration dresses — `Element::boundary`, and the three
mechanisms behind it — stays in the README, because a glyph boundary is
one of three answers and the other two are about shapes and images.

### Text fx

Motion inside a text leaf is a list of **tracks**. One `Track` is five
values — *which* glyphs (`Selector`), *what* deviation from rest
(`TextEffect`), *how* the beats spread (`motion::Spread`), what a unit IS
(`Track::over`), and the master `Animatable<float>` progress that drives
it. The spread is SigilMotion's and says nothing about text; `over` is the
whole of what makes it a cascade over glyphs rather than over a set's
children or a feed's rows. `Element::fx` appends one;
several compose per glyph, with `GlyphMod` offsets and rotations adding
and scale and alpha multiplying.

```cpp
text(u8"ONE LINE, TWO MOVES", display)
    .fx({.effect = fx::rise(20), .over = unit::Word})
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
first — `Track::innerOver` says what a unit is at that second level.

**Irregular timing.** `motion::Spread::cues` replaces the even spread with
a TABLE — one start time per unit, in ms — which is what caption, lyric
and lip-sync timing actually is:

```cpp
text(lyric).fx({.effect = fx::rise(12),
                .stagger = motion::Spread{.durationMs = 180}
                               .cues({0, 340, 720, 1180}),
                .over = unit::Word});
```

It answers the spread itself, so it goes anywhere one goes and compares
like one. A table says only *when unit k starts*; `durationMs` and `then`
are untouched by it, while `eachMs`, `amountMs`, `from` and `distribution`
have nothing left to say and are ignored. A unit past the end of
`motion::Spread::cueMs` starts at the last entry (the tail piles, visibly, rather than being given
times nobody wrote), entries past the last unit go unread, and either
mismatch warns once.

**Which list the beats are numbered against.** `Track::beatsOver` takes
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
text(field, rain).fx({.effect = streak, .stagger = cascade,
                      .over = unit::Line, .innerOver = unit::Cluster,
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
// emberDissolve is a SigilMaterial recipe over the params struct Burn,
// carrying the pass body as its SkSL.
auto burn = Material::recipe(sigil::material::Material(emberDissolve,
                                                       Burn{ink}));
text(u8"EMBER DECODE", display)
    .fx({.effect = fx::pass(burn), .stagger = {.eachMs = 260}});
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
    .onPath({.path = geometry::shapes::circle(),
             .at = &phase,                       // the marquee
             .align = TextPath::Align::Center,
             .orient = TextPath::Orient::Tangent})
    .fx({.effect = fx::rise(18)});
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
through `core::env::Provide`. An explicit set beats the inherited one whichever
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
then draws through the batched glyph path, where a span style's band
stands at its rest placement while the letters move. Anything else the
style changes — another face or size, an axis the face moves advances on,
an axis the text was shaped with and the restyle drops — is a reshape; and
an earlier axis-only restyle under a later reshaping one over the same text
re-shapes too, so the later declaration is the one that stands.

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
`Element::maxLines`, `Element::lastLine`, `Element::justification`,
`Element::tabStops`, `Element::reserve` and `Element::live` set the
general knobs of `sigil::weave::ParagraphLayoutOptions` on any content
form; `Element::kinsoku`, `Element::hanging` and `Element::mojikumi` hand
it the three tables a house's own setting is stated in, and
`Element::lineBreakLocale` names the tailoring the segmentation runs
under, which belongs to the Paragraph and lands there the way
`writingMode` does. The rest of that struct — Knuth-Plass tolerance,
line-metric overrides — stays behind the paragraph overload, which takes
the whole options value. **On that overload the setters override FIELD BY
FIELD**, and only the fields actually set: everything a setter did not
name keeps the value that was passed in.

SigilWeave's `FEATURES.md` carries the control-by-control table, with the
verb or field on this side that reaches each one; it is the fastest
answer to "how do I set X".

### A passage whose input moves

**Settled text is the special case, not the moving kind.** `Element::live`
is a leaf saying that an input of its layout moves — a measure that
animates, a frame that grows, content that changes between frames — and
it buys two things: the break decisions of a block set in a uniform
measure are kept and reused, so a measure already crossed costs no
decision at all, and the block is broken against the measure alone rather
than against the frame's supply of lines, so a frame that changes only in
DEPTH changes which lines it holds and never where they break.

```cpp
text(caption, body).width(Dim(measure)).live(true, 2000.0f)
```

**NOTHING INFERS IT.** A live layout answers the overflow tail
differently from a settled one, so a guess would change the setting of a
page that never moves. A passage that moves says so.

The second argument is the composer's budget in microseconds: a block the
optimizing breaker cannot finish inside it is filled greedily for that
frame, and a degrade drops the whole setting rather than the breaker
alone — the hyphens, the justification passes past the word gaps, and the
widow rule go with it. **A degrade is provisional.** The leaf does not
hold that layout as the answer for its measure, so the next frame lays
out again and the setting comes back the frame the budget is met.

`Composer::settling(key)` is what the frame actually got: `reused` blocks
answered from decisions already made, `degraded` blocks the budget forced
to the greedy breaker. It is a REPORT about one input and not a verdict
about the node — the runtime holds one proof that a node has settled, and
this is folded into it beside everything else the node reads. What the
proof takes from it is one bit: a live passage that still composed this
frame can be set differently the next one with no number on the node
moving, which no value memo can see, so it declares like a live material
does. A passage answered entirely from the store is set exactly as the
frame before it, and caches.

### Paragraphs, frames and stories

A hard break inside a passage separates **blocks** — the paragraphs a
reader sees — and `Element::paragraphs` says how each one is set, one entry
per block in block order:

```cpp
text(rich(body).add(u8"A heading\nand its body, which runs on\nand on"))
    .width(Dim(360.0f))
    .paragraphs({headingStyle, bodyStyle})
    .firstBaseline(sigil::weave::FrameOptions::FirstBaseline::kCapHeight)
    .distribute(sigil::weave::FrameOptions::Distribute::kJustify);
```

`sigil::weave::ParagraphStyle` carries the leading, the air before and
after, the four indents, the keeps and whichever of the leaf's own
alignment, justification, hyphenation and tab stops the block overrides;
SigilWeave's README is the canon for what each one means. A block past the
end of the list is set by the leaf's own settings alone, so ONE entry
styles the first block and leaves the rest plain. `Element::paragraph`
sets every block alike, and `Element::paragraphs` also takes NAMES,
resolved through the `sigil::weave::ParagraphStyleSet` the environment
offers — the same discipline `rich().add(text, name)` follows for
character styles. A name no set in scope carries WARNS ONCE and the block
is set in the set's base entry, because a block quietly set in a default
nobody asked for looks exactly like a style that did not take.

`Element::firstBaseline` and `Element::distribute` are the two decisions a
FRAME makes that no line makes for itself: where baseline 0 sits below the
top of the box, and what becomes of the room left over down it.

**A story fills as many frames as it is given.** `Story` is content plus
its block styles and nothing else — no layout, no cursor, no frame — and
`frame(story)` is one text leaf over it, which `Element::key` names and
`Element::thread` links to the next:

```cpp
Story article(rich(body).add(u8"…"));
article.paragraphs({headingStyle, bodyStyle, bodyStyle});

root.child(frame(article).key("a").thread("b").width(Dim(300.0f)))
    .child(frame(article).key("b").width(Dim(300.0f)).ellipsis(u8"…"));
```

Each frame fills from where the one before it stopped, so the cut moves as
any frame's measure moves, and the blocks are numbered from the STORY's
start — the third block is set the same way whichever frame it lands in.

**A STORY NUMBERS ITS OWN LINES.** `sel::line(40)` is the fortieth line
of the story wherever it landed, so a chain that reflows moves the
selection with the text instead of addressing a different line in every
frame; words, characters, sentences and named runs were the story's
already, since every frame builds the whole story's paragraph and resumes
at a word. `sel::inFrame("b")` is the frame-local address beside it —
everything the named frame holds, and nothing anywhere else — so
`sel::inFrame("b") & sel::line(40)` is "line 40, if frame b is where it
landed". A frame-local address on a leaf with no `key` can never match and
warns once.

**BEATS SPAN THE CHAIN.** A cascade over a threaded story runs one clock
across the whole of it: with `beats::Text` the fortieth word is beat forty
wherever it landed, so a staggered reveal carries on from one frame into
the next instead of restarting, and a `fx::seq` phase's crossfade stays
put across a reflow that moves a word from one frame to another —
its beat is the story's, not the frame's. The word, the sentence and the
line are the three granularities this holds for, because each carries a
story ordinal on the placed glyph. A CLUSTER AND A GLYPH DO NOT: their
ordinal is a position in this frame's walk, so a cascade over either
restarts at each frame.
Overflow on any frame but the last is the normal case and draws no marker,
whatever ellipsis the leaf asked for; the last frame is the one that
threads nowhere. A frame's own geometry is its business: it may flow
around a silhouette or carry exclusions like any other text leaf.
`kit::columns` is N frames side by side threaded in order, which is what a
Western multi-column measure is — the vertical writing mode keeps the word
column for the thing it already meant.

The chain is walked in the derive pass, in chain order, with each frame
re-filled at the measure it resolved to before the next is asked what it
inherits — so a chain of any length settles in one pass rather than one
link per convergence round. A chain that closes on itself stops where it
closes, as a cyclic borrow does. The walk also tells each frame the
measure the frame AFTER it resolved to, which is the one fact the widow
rule needs and no single fill can see: the lines a widow rule counts are
the remainder, and the remainder is set in the next frame.

### Beside the text

`Composer::units` is what everything standing next to a passage is placed
from: one `TextUnit` per unit a selector addresses, in draw order, in the
composer's space.

```cpp
for (const TextUnit &u : composer.units("verse", sel::each(unit::Word),
                                        unit::Word))
  ;  // u.rect, u.axis, u.pitch, u.ascent, u.range, u.style, u.lineIndex
```

`Beat` is the same rect under a schedule and needs an `fx()` track to
report it; this needs none, and carries the baseline (or the column's
axis), the pitch, the face's band, the writing mode, the vertical form,
the text range and the style beside each rect. It is read off the
placement rather than measured again, so a unit whose base broke across
two lines reports TWO entries, on the two lines.

Two things are built on it, and which one a case wants is decided by one
question — does the annotation need ROOM?

- **`Element::annotate`** is part of the text. Its band is put into the
  base's strut BEFORE the base is broken, so the pitch opens once and the
  reading is placed on the result; nothing chases anything. Ruby and
  kenten are `Annotation` values, and mono, group and jukugo ruby are the
  UNIT choice and nothing else. `kit::ruby` and `kit::kenten` are the two
  stock spellings. The PLACEMENT is SigilWeave's — the band a reading
  needs, where it stands against its base, and how a broken base shares
  it — and this tier only says which units are annotated with what.
- **`kit::annotate`** is a sibling that reserves nothing and stands beside
  the finished text — marginalia, word labels, callouts. It resolves at
  describe time from the layout the last draw left standing, on the same
  terms as the instruments, so a text that reflows wants a re-describe for
  its annotations to follow.

`kit::annotate` says where an object goes in one of two ways, and they are
one mechanism with two placement values. `kit::Beside` does the arithmetic
of the READING DIRECTION — before or after the unit across it, at its
start or its end along it — so a note above a line and a note right of a
column are the same declaration. `kit::Anchored` hands the arithmetic to
the caller: the object is still tied to a text position and still moves
when the text reflows, but it stands at an offset the author states.

```cpp
kit::annotate(composer, "verse", sel::text(u8"Ishmael"), unit::Word,
              {.horizontal = kit::Anchored::From::Frame, .offset = {-44, 0}},
              [&](const TextUnit &u) { return figure(u); });
```

`kit::Anchored::horizontal` and `kit::Anchored::vertical` name what each
AXIS is measured from — the unit, the whole line it landed on, or the text
node's frame — separately, because the commonest anchored object in print
takes its x from the frame's edge and its y from the word it belongs to.
`kit::Anchored::at` picks the point of those rects to measure from, as
fractions, and `kit::Anchored::offset` how far. The offset is in the
composition's axes rather than the reading direction's, which is the whole
difference between the two values.

`kit::rules` cuts a rule or a shade to the extent a block's lines actually
occupy, `kit::bullets` hangs markers in a hanging indent, and
`kit::dropCap` is an initial with the body flowing around it — an ordinary
exclusion, resolved in the ordinary pass.

A NESTED STYLE — the opening of a paragraph set differently from the rest
of it — is a selector and a span restyle, and `kit::NestedStyle` is the
statement of where it stops: `kit::NestedStyle::Until::Words` counts the
paragraph's own words, `Until::Characters` counts a character range, and
`Until::Delimiter` runs through the first occurrence of a mark, inclusive.
`kit::nestedRun` answers the `Selector` that means, `Element::spanStyle`
does the work, and `kit::dropCap` takes one so an initial and the small
caps that carry a paragraph out of it are written together.

```cpp
kit::dropCap(u8"W", capType, rest, bodyType, "dropcap", 6.0f,
             kit::NestedStyle{.count = 3, .style = smallCaps});
```

Because it is a selector, the run re-resolves with the text: an edit that
adds a word before the delimiter extends it, and one that removes the
delimiter leaves it covering nothing rather than covering the paragraph.

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
    .fx({.effect = fx::rise(24)});
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
track with `.over = unit::Line` beats column by column and `sel::line(0)` addresses
the rightmost one; `unit::Cluster` runs down a column in reading order.
`spanPaint`, `spanStyle`, `textAlign` (start is the top of the column),
`maxLines` (which clamps COLUMNS) with `ellipsis` at the clamped column's
foot, `flowAround`, `lastLine`, `lineBreak`, `textStroke`,
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

**`flowAround` and `ellipsis` follow the type down the page.** An exclusion
cuts a COLUMN exactly as it cuts a line: the column a target crosses hands
back a head above it and a foot below it, and the same silhouette is
subtracted — a `shape()` outline, an analytic circle, or the box a target
that declared none stands in — with the margin the same standoff in all
three. And a clamped column ends in its marker, at the column's FOOT,
measured against the column's length so the cut moves up to make room for
it. The marker stands for the text it cut and is set the way that text was
set: upright after upright glyphs, in the face's own vertical form when it
has one, and turned with the column after a rotated Latin run.

**What does not follow the type down the page.** `onPath` ignores
`writingMode` entirely — a path run's baseline is its own geometry and has
no columns to advance — and setting both warns once and keeps the path. A
decoration on a span DOES follow the type down the page — an underline runs beside the
column on its right, an overline on its left, a strikethrough down the
column axis and a highlight across the whole column pitch — but it never
skips ink there, because ink intercepts are cut out of a horizontal band
window that a column's band is not. Which side an underline or an overline
takes is `side` on the decoration: the default puts a column's underline on
the right, the side a vertical setting reads its emphasis line on, and
`Decoration::Side::kOpposite` is the other placement (left of the column,
above the line).

**A BAND UNDER A TRACK STANDS AT REST**, in either writing mode. A track
draws its own glyphs in batched buckets and a bucket carries glyphs alone,
so the band is drawn beside them from the layout the letters left at rest:
the letters travel on their schedule and the band does not travel with
them. That is the same stand `mark()` takes — a rect resolved from the
layout cannot chase a paint-time pose — and it is the honest one for a
band, which dresses a whole run rather than one letter. Type on a path
carries no band either way: a turned run's band would have to follow the
curve it rides.

**Ruby and kenten are `Element::annotate`**, in a column exactly as along
a line: the band a reading needs goes into the base's strut before the
base is broken, and the reading is then placed on the result, on the side
the writing mode reads its furniture on — above a line, to the RIGHT of a
column. `kit::ruby` and `kit::kenten` are the two stock spellings, and
mono, group and jukugo ruby are the unit choice and nothing else. See
"Beside the text".

---
