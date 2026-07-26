# SigilCompose — grammar & ergonomics audit (evidence file)

> **Evidence file for `../ROADMAP.md` §33** (2026-07-25). Provenance,
> not spec: candidate spellings here are PROPOSALS awaiting the
> designer's taste calls; nothing below is decided unless §33 or the
> canon says so. Counts read s/g/t = sketches / gallery headers /
> test TUs. Canon under test: "the grammar names the author's intent,
> never the mechanism" (DESIGN.md §Growth rules).

## The meta-rule the audit produced

**When a doc comment's job is to distinguish two names, that is the
rename ticket.** Brushes.h spends 49 lines on `cornerAlign`, 32 on why
`Rails` exists, 25 on `dashGeometry`; Lines.h needs 21 to defend a
`circle()` overload; Decorations.h needs 15 to separate
`PathFormat::trimStart` from `Element::trim()`. Every one of those
paragraphs is a name that did not carry its own meaning, written after
a study shipped wrong.

## Class 1 — mechanism names leaked to authors

- **M1 `PropValue<T>`** — says "property value", means "may be
  constant, ramp on change, be driven, or be shaped-driven". NOT
  internal: 12/35 sketches spell it (cde_motif.cpp:456-492 etc.).
  Candidates: `Animatable<T>` (recommended), `AnimatablePropValue<T>`
  (greppable, keeps the mechanism word), `Motion<T>` (wrong — three of
  four forms aren't motion). Churn 59 author sites, 0 forced via
  `using` alias. RENAME alias-first.
- **M2 `bind()`/`Bound`** — mechanism verb for the DRIVEN door.
  Candidate `drive(&out)` as primary, `bind()` stays alias. 168 sites.
  RESPELL.
- **M3 volatility has five spellings** — `animated()`, `animatedWalk`,
  `animatedMod`, `Material::isLive()` + `Material::animated()` (both,
  one class), `Cache::None`. One contract ("repaint every frame; do
  not cache"). Candidates: `isLive()` unify / `runsItself()` /
  `repaintsEveryFrame()`. ~30 sites, mostly library-internal. RENAME
  alias-first.
- **M4 `PaintContext::animating` is DEAD** — Compose.h:463, `= false`,
  never assigned; Brushes.h:459/482 copy false forward. Every custom()
  reads false forever. Wire it (volatility is already computed) or
  delete it. VERIFIED 2026-07-25.
- **M6 `Pool::touch()`** — the MANDATORY Data-mode step, called once
  in the whole corpus; `revision()` zero. Candidates `changed()` /
  `commit()`. Churn 1 — free, and a correctness win. `Pool`/`Atlas`
  themselves: correct jargon, DOCUMENT (100+ sites).
- **M7 `sdf::Shape::p0/p1/p2`** public uniform slots — hide behind the
  factories. Churn 0.
- **M8 lower value**: `console::LineRing` (author has a *log*),
  `Cache::Group` (names nothing; means "bake container + settled
  animated children"), `GeometryOp`/`GeometryScheme`/`ops::PathOp`
  (three names for one idea), `Stats::picturesRecorded` (header
  admits it misled a reader).

## Class 2 — empty or misleading verbs

- **E1 `with`/`withFrom`/`withKeyframes`** — the family is INVERTED:
  `with(` 17 total vs `withFrom(` 294. The base verb is nearly dead;
  the extension is the workhorse. `withKeyframes` also never deduces
  its template arg (all 17 sites write `<float>`). Candidates: door
  verb pair (`changes(v,spec)` / `entersFrom(a,b,spec)`), or
  value-site spec `opacity(v, ease)` (covers change, not entrances;
  `ramp` is taken by studio::ramp, 168 sites). ~311 sites. RENAME
  alias-first BEHIND the §32 probe (port chaucer_astrolabe 48
  withFrom / chevreul_circle 29 bind / spacejam_1996 4 uTime; read).
- **E2 `.stroke(util::stroke(...))` stutter** — 82 sites of the
  stutter; setter is right (keep), helper renames (`util::rule` /
  `util::pen`). 211 helper sites, alias-able.
- **E3 `Brush::op()`/`leg()`** — nouns as verbs; `leg()` undersells
  (only way to add paint). Candidates op→`bend`/`via`,
  leg→`pass`/`paint`. ~58 true uses; cheap now, expensive later.
- **E4 `styles::innerGlow()` returns InnerShadow** (churn 1 — RENAME);
  `dropShadow()` returns util::Shadow (rename doing real work — KEEP).
- **E5 `Pattern::seed(n)` mutates SHARED state** (re-bakes copies)
  while sibling setters are per-object. Material::uniform already
  solved this copy-on-write. RESPELL (`reroll(n)` or make seed COW).
  14 sites.
- **E6 `Composer::render()`** means reconcile, canon phase name is
  Describe; 484 sites + React lineage → DOCUMENT, noted for record.
- **E8 `Element::echo()`** — audio word for a print-registration pass;
  its own doc says "registration-error language". Candidates
  `misprint()`/`underprint()`. 9 sites. RENAME.
- Shorter list: `Atlas::cell()` (registers + mutates + drops sheet),
  `gpuimg::ready()` (adjective that uploads), `shapes::has(a,b)`
  (maximally empty, ADL-visible), `brushes::restyle` (op-first order
  is a documented trap), `console::panel` vs 50 hand-rolled `panel(`,
  `Element::style()` (emptiest word for "splice two vectors").

## Class 3 — implicit mode switches

- **I1 `fill(&out)`** — one corpus site EVER; a study concluded bound
  Fill didn't exist and rebuilt on renderSlot. RESPELL under the
  drive verb; retire bare-pointer overloads (§32).
- **I2 SkSL text selects volatility tier** (`uTime`→live,
  `uResolution`→geometry). Keep inference, ADD explicit spelling
  (`.live()` / `.perFrame()`). 56+ sites read the inference.
- **I3 `stack()` sets absolute on EVERY child unconditionally**
  (Reconcile.cpp:574; adjacent comment claims otherwise — fix the
  comment). DOCUMENT.
- **I4 `slot(name)` stores name in `key`** — `.key()` on a slot
  silently renames the mount; renderSlot no-ops. Latent (0 realised).
  DOCUMENT or assert.
- **I5 `Mode::Live` = Cache::None propagating refusal upward** — and
  it's the POPULAR mode (25/4/1 vs Data 8/0/5). Candidates
  `Mode::PerFrame`/`Uncached`. RESPELL.
- **I6 `ops::wave` vs `ops::Wave`** — case-only, opposite pruning
  (lambda never prunes; struct does), vs canon's own comparable-values
  rule. Lowercase family ~10 sites; `ops::rounded` 0 uses AND collides
  with `shapes::rounded`. DELETE lowercase family.
- **I8 `Placement::interval == 24.0f` sentinel** — Brushes.h:716: an
  author explicitly writing 24 gets silently overridden by `spacing`.
  `optional<float>` (cornerAlign precedent). VERIFIED 2026-07-25.
- **I9 `Ribbon::widthFn`** — one assignment changes four semantics
  (overrides ends + nib, kills equality/pruning, makes widthMax
  load-bearing). Factory `Ribbon::variable(widthFn, widthMax)` makes
  the coupling unforgeable. 20 sites.
- **I11 sentinel catalog** (each one line in source): nibAngleDeg=-1;
  advance=0/cornerLength=0 two meanings of zero; `alongStops`
  disabling parallels+dashes; caps trimming the body; non-const
  `Pool::sizes()`/`texWindows()` materializing lanes; ringExtent=0
  topology switch; SIZE_MAX palette; shadow alpha 0 changing pad();
  sector/annulus topology + winding; Inset sign; `outline()` silently
  overriding `corners()`; Overlay freezing live materials silently;
  `Transition{360ms,{},220ms}` throwing on frame one.
- **I12 decorations have a SECOND binding grammar** — raw `Output*`
  fields (`dashPhaseBinding`, `bindOffsetX`, `spacingBinding`...);
  Lines.h:1242 admits "the odd one out". DOCUMENT now; unify under
  the door verb later.

## Class 4 — vocabulary inconsistencies

- **V1 Fill vs Material: five peer types, five positions** —
  Element::fill both / textFill Material-only / textStroke Fill-only /
  PathFormat both / Border+Line+Rail+Ribbon+Hatch Fill-only.
  PathFormat's doc records "writing the same brass twice, once per
  return type." Fix: ADDITIVE Material overloads everywhere. Churn 0.
  (Part of decision C.)
- **V2 `linear` vs `linearUnit`** — suffix names the mechanism; corpus
  wants unit-space 8:1 in sketches; radialUnit's half-diagonal
  convention caught two studies. Alias-in new names, NEVER redefine
  (§27). 256 sites.
- **V3 "corner" means seven things** — and `Corner::All == 15` while
  `Corners{15}` compiles (radius 15). One letter apart, overlapping
  scope, opposite meanings. Rename the mask: `CornerMask` /
  `AtCorners`. 33 sites.
- **V4 three `Align` enums** — flex / stroke position / run placement.
  Kernel keeps the word; `PathFormat::Align`→`Position` (Figma's
  term), `TextPath::Align`→`Place`. 178 extension sites.
- **V5 `bleed()`/`reach`/`extraBleed`** — one idea, three names, same
  structs. Unify on `bleed` (the kernel's word).
- **V6 `decorations::brackets`/`gappedRule` vs
  `lines::cornerBrackets`/`cornerGaps`** — same capability, corpus
  found one (41 vs 3 uses). Sibling-path failure in naming form.
- **V7 `rail` is two systems** — Element rail() (routed path) vs
  lines::Rail(s) (parallel stroke line). Rename lines' to
  `Strand(s)`/`Course(s)`. ~50 sites.
- **V8 `Pattern`/`patterns::`/`PatternBrush`** — and Patterns.h:5's
  claim "Each returns a Pattern" is FALSE for its three most-used
  entries (halftoneRamp/noise/grain return Material; grain alone 64
  uses). Fix doc immediately; `Pattern`→`Tiling` candidate. 67 sites.
- **V9 `Kinetic.h`→`glyphfx::`→`GlyphFx`→`.glyphFx()`→"kinetic"** —
  four spellings of one idea; rename header to GlyphFx.h (15 incl).
- **V10 duplicated type names** — Line ×3, Mode ×3, Kind ×3, Style ×2
  (+LayerStyle+styles+.style()), Span ×3, Anchor ×2, Cell ×2,
  Coverage ×2, Grid ×4 concepts, `Radial` ×2 with OPPOSITE meanings,
  circle ×3 returning three types, star ×2 incompatible params.
- **V11 spacing/interval/advance/gap/step/pitch** — six words for
  "how far apart"; `gap` REVERSES meaning between Line::gap
  (centre-to-centre) and cornerGaps/gappedRule (paper omitted).
- **V12 blur = size/sigma/blur/radius** across four APIs; `innerRatio`
  means three things.
- **V13 sampling ×3 spellings** (Element::sampling / filter fields /
  Material image arg) — the class of failure Compose.h:991 admits.
- **V14 orthography** — waveLength vs wavelength; centre vs center;
  sdf Kind::Star vs sdf::star() capitalisation-only.
- **V15 `util::linearGradient/radialGradient`** — 13 uses vs
  Material's 256. DEPRECATE.
- **V17 `LayerStyle::under/over` vs `background/foreground`** — one
  axis, two vocabularies, translated inside one call.

## Class 5 — names that undersell

- **U1 derive family unreadable as a family** — `flowAround`
  (preposition, method) + `connector` (agent noun, free fn) + `rail`
  (transit metaphor) + `routers::` (matches none); Routers.h mixes two
  incompatible router kinds with no signal. ~55 uses total — the low
  churn IS the symptom. One prefix/namespace when the derive-export
  seam work lands.
- **U2 Layouts.h** — decision-schemes correctly unused (keep); the two
  CHORES are buried: `BaselineGrid` (only consumer of
  LayoutInput::childBaselines, omitted from API.md per §25) and
  `ModularGrid`. Candidates textRhythm/columns.
- **U3 `kit::Frame`** — highest-value kit type; "frame" collides with
  picture-frame and animation-frame. → `Dial`/`Polar`. 24 sites.
- **U8 `artAlong`/`ArtBrush`** — the only continuous warp; 3 uses. →
  `bendAlong`/`warpAlong`.
- **U9 `styles::textGlow`** works on any layer → `styles::glow`.
- **U10 `shapes::inset`** collides with `Element::inset()` →
  `insetBy`; `onEdges`/`edges` adaptors under-documented.
- **U12 `util::Stage`** — "the canonical three-line host loop" has ONE
  user (a test). Discoverability, not naming.
- **U14 stamped-brush public `cache` field** — three copies of the
  same warning in one header; bake identity should ride a constructor.
- **U16 style presets** (aquaGel/y2kChrome) have zero sketch uses —
  authors reach past presets to raw structs.
- **U18 the missing name, caught red-handed** —
  chaucer_astrolabe.cpp:972: "there is no clipOut() and no
  shapes::subtract" — an author reaching for two spellings that don't
  exist. Boolean shape vocabulary is absent surface, not a rename.

## Rejected (names that look mechanical and are right)

`custom()` (the mechanism IS the intent — escaping to raw Skia),
`key()` (author-owned identity, 666 sites), `cache()`/`bakeScale`
(deliberately buying the mechanism; the enum is the price tag —
except `Group`, M8), `memo()` (React vocabulary the library adopts),
`stack()` (right name; I3 is the finding), `trim()`/`wipe()` (terms of
art from motion/video), the query family (snapshot/measure/metrics/
bounds/hitTest/paragraphLayout), the CSS/flex vocabulary,
`PaintContext::outline`, `Bound::window` (name right; the DEFAULT
should maybe flip — §27), `Promotion::` reasons (naming causes is the
intent), the whole `studio::` prelude, `util::centred/disc/marquee`,
`decorations::paintOn`.
