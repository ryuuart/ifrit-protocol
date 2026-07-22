# EXTRACT — what the corpus built by hand, and what the layer above should be

Phase-2 inventory, merged and arbitrated. This supersedes the separate
sketch and gallery inventories; where they disagreed, the disagreement is
resolved here **against the source**, and the resolution is shown.

## The two populations

| | files | lines | comment | character |
|---|---:|---:|---:|---|
| sketches (`sketch/sketches/*.cpp`) | 35 | 54,516 | 11,076 | one artefact per file, one author, one sitting, all to a common brief |
| gallery (`gallery/*.h`, `*.cpp`) | 31 | 10,585 | 1,490 | older, mixed subjects, the repo's house-style reference |
| **combined** | **66** | **65,101** | **12,566** | |

**Nine of the twenty-one gallery scene headers are ports of sketches** —
verified, every one says so in its own header comment: `ScenesConsole.h`,
`ScenesBeethoven.h`, `ScenesAero.h`, `ScenesNetwork.h`, `ScenesKinetic.h`,
`ScenesPoster.h`, `ScenesPersona.h`, `ScenesY2k.h`, `ScenesZellige.h`
(each `:2-5`, "ported from sketch/sketches/…"). Their source sketches no
longer exist in the tree, so the port is the only surviving copy — which
means they carry the sketch population's spelling and **are not
independent evidence**.

The independent gallery subset is twelve headers: `ScenesInventory.h`,
`ScenesSkillTree.h` (+`SkillTreeData.h`), `ScenesCosmati.h`,
`ScenesVeloren.h`, `ScenesGerstner.h`, `ScenesFlourish.h`,
`ScenesOrnament.h`, `ScenesOrganic.h`, `ScenesScale.h`, `ScenesGame.h`,
`ScenesChrome.h`, `ScenesData.h`.

**Every cross-population claim below is re-counted against that subset,
and marked.** An earlier draft treated the gallery wholesale as a control
group; for nine of its files that was unsound, and the numbers here are
the corrected ones.

*Counts are a snapshot taken 2026-07-22 while `thunder_fulu`,
`bg3_dice_roll`, `eva_magi_interior` and `dunhuang_star_chart` were
landing. No sketch or scene was edited.*

---

## 0. Do this first: 1,316 dead characters, zero risk, no design decision

Verified at `Compose.cpp:88-129` — `inset`, `left`, `top`, `right`,
`bottom` and `centerAt` **each set `layout.absolute = true` themselves**:

```cpp
Element &Element::left(Dim d) {
  m_node->layout.absolute = true;      // ← and identically in the other five
  m_node->layout.hasInsets = true;
  m_node->layout.insets.left = d;
  return *this;
}
```

So an `.absolute()` adjacent to any of those six is a no-op. Counting on
the whitespace-stripped corpus (a call counts as dead only when it is
*immediately* adjacent to an edge setter, so this undercounts rather than
inflates):

| | total `.absolute()` | dead | load-bearing | % dead |
|---|---:|---:|---:|---:|
| sketches | 1,091 | 1,039 | 52 | 95.2% |
| gallery | 288 | 277 | 11 | 96.2% |
| **combined** | **1,379** | **1,316** | **63** | **95.4%** |

Worst files: `minard_1869.cpp` 129/129, `chaucer_astrolabe.cpp` 85/94,
`dunhuang_star_chart.cpp` 84/84, `thunder_fulu.cpp` 71/71,
`ScenesAero.h` 64/64, `sigillum_aemeth.cpp` 55/64, `ScenesVeloren.h`
45/45, `ScenesInventory.h` 31/31.

The 63 survivors are real: 48 are `.absolute().width(…)` — a node that is
absolute but pins no edge — plus ten `.absolute().zIndex(…)` and a
handful of others. They stay.

**This is a mechanical rewrite with no behaviour change, no reconciler
effect, and no design decision, and it removes more lines than the entire
proposed layer removes from the two flagship gallery scenes.** It is
first in this document because it is the only item here that is purely
subtraction. Credit for spotting it belongs to the gallery inventory,
which found the gallery's 277; the combined 1,316 is the number to act on.

> The removal was already in flight when this merge was written — the
> counts above were taken before it started, so a later re-measure will
> return a smaller number and that is the fix landing, not an error in
> this table. What must survive the sweep is the 63: an `.absolute()`
> with no adjacent edge setter is load-bearing and deleting it silently
> un-absolutes the node.

---

## 1. The governing distinction

The library has already shipped two "layers above", and their outcomes
are opposite.

`<sigilcompose/Util.h>` — 206 lines of deliberately demoted sugar:

| helper | uses | files |
|---|---:|---:|
| `util::toU8` | 387 | 28 |
| `util::stroke(w, fill, align)` | 166 | 21 |
| `util::disc(centre, r)` | 89 | 10 |
| `util::radialGradient` | 11 | 8 |

`<sigilcompose/Layouts.h>` — 307 lines, seven placement schemes. Five
uses corpus-wide, all in the gallery; **zero in 35 sketches**.

> **`util::stroke` names a value spelling. A layout scheme names a design
> decision.** Sugar over a spelling you were going to write anyway gets
> used 166 times. A component that decides *where things go* is used five
> times, because deciding where things go is what the author came here to
> do.

That line survives the merge and governs every verdict below. But the
evidence underneath it needed correcting, and the corrected version is
sharper.

### 1.1 What the `Layouts.h` evidence actually says — corrected

An earlier draft read the five uses as "`Layouts.h` failed". **That does
not survive the control group.** Read at the call sites:

- **`ScenesFlourish.h:225` uses `Radial` and it works.** The `kPetals`
  petals built at `:198-203` are byte-identical — same
  `width(13).height(18)`, same star outline, same fill, same stroke.
  Radial's contract (evenly spaced, container-relative, rect-only) is
  exactly the requirement. Zero friction.
- **`ScenesOrganic.h:71` uses `Radial` and it works.** Nine runes around
  a star sigil; the per-instance variation is *content only* (a different
  glyph string). Position, size and style are uniform.

So `Radial` is not a failed abstraction. It is a **correctly scoped** one
that succeeds on its stated case and is declined everywhere else — and
the reason it is declined is specific and already documented:

**`LayoutScheme::place` returns `std::vector<SkRect>` and nothing else**
(`Layouts.h:43`). `ScenesCosmati.h:394-410` hand-places four roundels and
four guilloche arms on the diagonals, where `Radial{.startDeg = 45}`
would have been geometrically perfect, because each instance also needs
`.rotate(a·180/π)` (`:274`) and a per-index entrance delay
(`delay{140 + 90·seed}` at `:229`, `delay{420 + 70·seed}` at `:272`).
The library says this itself, at `Layouts.h:201`:

> *"Placement stays yours (LayoutScheme can't set rotations)."*

**The corrected finding is therefore narrower and far more actionable
than "Radial is wrong":**

> A layout scheme returns rectangles, and real placement is
> (rect, rotation, phase). Any figure with per-instance rotation or a
> per-index entrance delay must hand-place — which is most figures in
> this corpus, and none of the ones `Radial` was built for.

That is a `LayoutScheme` limitation worth a ROADMAP entry and a doc
comment at `Layouts.h:38`, not a replacement layer. Two of the four
scenes an earlier draft named never face the question at all:
`ScenesZellige.h:117-120` is `box().row()` over three panels, and
`ScenesOrnament.h`'s radial content is drawn inside a `PaintProgram`
(`OrnamentKit.h:94`, `:177`), not placed as elements.

### 1.2 The corpus's own worked example of a wrong abstraction

Not `Radial`. **`layouts::stickerScatter`** (`Layouts.h:198-237`), a
27-line generator for the ATLUS sticker ladder: **zero users in either
population.** The one scene that needs a sticker scatter —
`ScenesPersona.h` — refuses it and transcribes the verified ladder from a
Godot recreation instead (`:87-104`, `:111-121`). And the header concedes
why, in its own doc comment at `Layouts.h:208-210`:

> *"The verified d[e]lt[a] ladder {−25,−15,−20,−15,…,+8} remains the
> hand-authored reference; this generates ladders in that family."*

**A generator that produces things "in the family of" the reference is a
choice made cheap, and the corpus voted with its feet.** This is the
best-measured "don't" available, it is better evidence than anything in
§5, and it should be the standard the next abstraction is held to.
`stickerScatter` is a deletion candidate.

---

## 2. What recurs — measured, ports excluded

### 2.1 The prelude — the strongest candidate, and the only one both populations demand

| helper | sketch defs | gallery defs | of which independent | total lines |
|---|---:|---:|---:|---:|
| `type(…) -> weave::TextStyle` | 23 | 18 | **7** | 430 |
| `hex` / `rgb` / `C` `(0xRRGGBB, a)` | 19 | 5 | **4** | 101 |
| `face(family, …)` fallback chain | 15 | 1 | 1 | 168 |
| `fmt(const char*, …)` | 7 | 0 | — | 53 |
| `ramp(delay, dur, ease)` | 6 | 0 | — | 43 |
| `mul`/`dim`/`mix`/`fade`/`alpha` colour ops | 11 | 1 (a local lambda) | 1 | 22 |
| `prog(PaintProgram)` | 3 | **0** | — | 3 |
| **total** | **84** | **25** | | **820** |

**Re-counted with the nine ports excluded, both headline claims survive:**

- `hex()`: **23 independent hand-rolls** (19 sketch + 4 gallery —
  `ScenesInventory.h:69`, `ScenesCosmati.h:48`, `ScenesGerstner.h:51`,
  `ScenesVeloren.h:55`; `ScenesY2k.h:51` is a port and is excluded).
  Three names — `hex` 12, `rgb` 7, `C` 4 — one byte-identical body.
- `type()`: **30 independent** (23 sketch + 7 gallery — `GalleryCore.h:35`,
  `ScenesCosmati.h:80`, `ScenesInventory.h:99`, `ScenesGerstner.h:96`,
  `ScenesVeloren.h:106`, `ScenesSkillTree.h:122`, `ScenesFlourish.h:72`).

Two further data points the sketch inventory did not have:

**The absence of `hex()` produced transcription risk, not just
verbosity.** `ScenesPersona.h:63-83` writes 22 float triples with the hex
in a trailing comment (`{0.0039f, 0.3725f, 0.8000f, 1}, // #015FCC`), and
`ScenesSkillTree.h:76-86` does the same for 11. That is **33
hand-converted constants a helper would have made checkable.**

**The shared helper already exists and failed on its signature.**
`GalleryCore.h:35` ships `styleAt(float size, SkColor colour)` and
**twelve scenes wrote their own anyway** — because it takes `SkColor`
where every palette is `SkColor4f`, and carries no tracking, weight or
condense. That is the design constraint on any replacement, and it is why
§4.2 proposes a designated-init aggregate rather than a positional
function.

**`prog()` is refuted.** Two `Decoration(…)` construction sites in the
whole gallery (`ScenesFlourish.h:153`, `:271`), and one of those wraps a
ContourWalk, not a paint program. The gallery deliberately does not have
the habit `prog()` serves — `ScenesPoster.h:4` states the goal as "ZERO
raw `custom()` lambdas", and value decorations are preferred because they
prune. **Extracting `prog()` would make the non-pruning path one word
shorter than the pruning path.** Dropped.

### 2.2 The colour-arithmetic longhand — corroborated as a need, not as a helper

61 inline sites in 10 gallery files, **33 of them independent**, with
exactly one helper written anywhere (a lambda local to one function,
`ScenesCosmati.h:102-105`):

- alpha override, `{c.fR, c.fG, c.fB, a}` — **45 sites**, 20 of which
  wrap to a second physical line (`ScenesSkillTree.h` 13,
  `ScenesInventory.h` 10, `ScenesGerstner.h` 5, `ScenesFlourish.h` 4, …)
- channel scale, `{c.fR*k, c.fG*k, c.fB*k, a}` — **16 sites**

The sketches' `mul(c, k, a = -1)` covers both. But the dominant case by
3:1 is the alpha override, and `mul` is a poor name for it — **ship two
names**, `alpha(c, a)` and `scale(c, k)`. ~50 physical lines.

### 2.3 The placement longhand — real, and narrower than it looked

| chain | sketches | gallery |
|---|---:|---:|
| `.absolute().inset(…)` | 323 | 169 |
| `.left().top().width().height()` | 389 (32 files) | 32 (4 files) |
| `.absolute().left().top()` | 154 | 33 |

**Nine files independently wrote the same three-line helper**, under four
names, in two shapes: `black_watch.cpp:544` and `chevreul_circle.cpp:406`
(byte-identical), `xcom_battlescape.cpp:842`, `spacejam_1996.cpp:244`
(`rect`), `fallout2_charsheet.cpp:384`, `winamp_base.cpp:248`,
`ksp_mapview.cpp:237`/`:242` (`place`/`at`), `twoadvanced_v4.cpp:410`.

And the studies that did *not* write it are exactly the ones carrying the
longhand: `chaucer_astrolabe` 53 quartets, `ds2_bench` 26, `astral_tome`
23, `bg3_dice_roll` 21, `thunder_fulu` 20, `vagrant_story_target` 18.

**But the ratio is the finding, and it scopes the fix.** In the sketches
`.left(` outnumbers `.inset(` nearly 2:1; in the gallery it is the
reverse, 1:2.6. The gallery is flex-native and the sketches are
coordinate-native, because **a study reconstructs an artefact by
measuring pixel coordinates off a reference plate, so its positions
arrive as numbers rather than as relationships.** See §3 for what that
costs the fix.

**An earlier draft claimed "no gallery scene wrote a placement helper (0
of 31)". That was wrong: fourteen did.** `pin`/`station`/`label`/`leaf`
(`ScenesNetwork.h:72,79,87,99`), `socket` (`ScenesSkillTree.h:174`),
`band`/`weaveSprig`/`panel` (`ScenesOrnament.h:69,78,227`), `moon`
(`ScenesOrganic.h:87`), `stud` (`ScenesInventory.h:350`),
`guilloche`/`quarter` (`ScenesCosmati.h:266,308`), `desktopIcon`
(`ScenesAero.h:603`), `medallion` (`ScenesFlourish.h:191`). What none
wrote is a *generic* one — and the reason is legible in the signatures:
by the time a scene knows what it is placing, the interesting parameters
are `key`, `seed`, `quadrant`, `rot`, `sdf::Style`, `dia`, not `w` and
`h`.

**That correction argues for the narrow fix and against anything larger.**
It is why §4.1 proposes exactly two methods and no widget.

### 2.4 The hand-accumulated clock — 36 files, both populations, and it is a missing accessor

New in this merge, and the most structurally uniform thing in the corpus.
**21 of 35 sketches and 15 of 21 gallery scenes (8 of them independent)**
open a steppable that accumulates its own elapsed time:

```cpp
ticker.add([this, t = 0.0](double dt) mutable {
  t += dt;
  …                       // named Outputs written here
  return true;
});
```

Inside them the same three signals recur under different names — a
wrapping phase `fmod(t·k, 1)` at 24 sites, a breath `0.5 + 0.5·sin(t·k)`,
a one-shot ramp `min(1, t/k)`. Nobody wrote a helper for any of them.

**This is not an extraction candidate; it is a hole.** Verified:
`FrameClock::elapsed()` exists (`FrameClock.h:43`);
`PaintContext::elapsedSeconds` exists; `Sketch::update(double elapsed, …)`
receives it (`Sketch.h:89`). But `Ticker::add` passes **only `dt`**
(`Ticker.h:41`), and `SketchContext` exposes no clock at all
(`Sketch.h:45-73`) — and a steppable is precisely where `ch::Output`s are
written. Thirty-six files therefore keep a second, private copy of a
clock the framework already owns, and every one can drift from
`PaintContext::elapsedSeconds` under pause or time-scaling.

**Wanted:** `Ticker::add` passing `(dt, elapsed)`, or
`SketchContext::elapsed()` / a `FrameClock&` on the context. That is an
accessor, not a layer. **File it in ROADMAP.** The phase/breath/ramp
helpers should wait until the clock is reachable — a helper over an
unreachable clock is how you get a thirty-seventh copy.

### 2.5 The verification console — 7 files

`chaucer_astrolabe.cpp:2507-2546` (4 panels, 2×2),
`sigillum_aemeth.cpp:1719-1775` (4, 2×2),
`thunder_fulu.cpp:1494-1533` (3, column),
`minard_1869.cpp:2531-2560` (5, row), `psx_doom_fire.cpp:565-605`,
`eva_magi_defense.cpp:1044-1065`, and `gallery/ScenesConsole.h:122`,
`:205`. Every one: a mono style repeated five times for the palette, an
absolutely-placed plate with a fill and an inset stroke, an inner padding
box, and dividers spelled as 1-px `box().fill(…)` children.

(`ScenesConsole.h` is a port, so the gallery does not corroborate this
independently — but six independent sketches do.)

117 `append(toU8(…))` calls; 21 carry a conditional palette index, i.e.
they are **checks**. `minard_1869.cpp:2580` invented the missing
primitive as a lambda, and `sigillum_aemeth` + `thunder_fulu` make 53
`fmt()` calls producing the same shape by hand.

### 2.6 The 1-bit mask bake — sketches only, and correctly so

`thaumonomicon.cpp:1134-1205` (72 lines) and
`vagrant_story_target.cpp:414-520` (107) share a 25-line core:
`measure` → `SkSurfaces::Raster` → `snapshot` → `readPixels` → threshold
to A8. **Zero gallery sites** — the gallery has no hand-rolled bitmap
font anywhere, because reproducing an aliased PS1/DOS artefact is a
study-specific need. (The gallery's analogue is the icon-path
dispatcher, which is content — §5.6.)

The cause is stated at `vagrant_story_target.cpp:409` and verified at
`Compose.h:1204`: **`text()` takes a `std::u8string`, not a
`PropValue`**, so a live numeric readout cannot be a text node.

And the shared core now exists — `debug::rasterize` (`Debug.h:315`),
added this run, whose own doc says *"Three studies hand-rolled the same
forty lines."* Neither bitmap study could use it: both predate it, and it
lives in a header named `Debug.h`.

### 2.7 Radial text — 24 sites, 8 files

The block, identical in `sigillum_aemeth.cpp:756-788` and
`chaucer_astrolabe.cpp:1661-1675`:

```cpp
text(toU8(str), style)
    .absolute().width(Dim(2*r)).height(Dim(2*r)).centerAt(centre).key(k)
    .onPath(TextPath{.path = shapes::circle(), .at = f, …,
                     .orient = TextPath::Orient::Radial})
```

The friction is `f`. `sigillum_aemeth.cpp:279`:

```cpp
float frac(float thDeg) { return std::fmod((thDeg - 90.0f) / 360.0f + 4.0f, 1.0f); }
```

Three angle conventions in one file — the figure's, Skia's, and
arc-length fraction — converted by hand at hundreds of call sites.

---

## 3. Would the layer help the house-style scenes? Mostly no.

Measured on `ScenesInventory.h` (791 lines) and `ScenesPersona.h` (707),
the two the repo's docs name as closest to house style. This question was
asked expecting the answer to be allowed to come back negative. It did.

| | `.left(` | `.top(` | `.centerAt(` | `.inset(` | lines |
|---|---:|---:|---:|---:|---:|
| `ScenesInventory.h` | 17 | 16 | 0 | 11 | 791 |
| `ScenesPersona.h` | 10 | 11 | 1 | 11 | 707 |

**Inventory.** At most 17 chains collapse, most spanning two physical
lines: a ceiling of ~34 lines, **4.3%**. And the coordinates are not
measurements — they are `cellX(item.col)`, `cellY(item.row)`,
`spanW(item.w)` (`:308-310`, `:474`), derived from a footprint table.
`rect()` respells the call; it does not remove the arithmetic. Its
prelude is 310 lines of which exactly 16 are generic (`C()` 5, `type()`
11) — **2.0%**; the other 294 are `artPath` (131 lines of item
silhouettes), `rarityColor`, `kItems`, `cellX/cellY/spanW`.

**Persona. Saving: zero lines.** Its placed nodes carry **no explicit
width or height** — `plainRow()` (`:438-447`, ten lines total) is
`box().key(r.label).left(nn::kBaseX + r.dx - 14).top(r.y - 14)
.padding(14)…`, sized by its text plus padding. `rect(SkRect)` cannot be
used at all. It already writes the terse form with no `.absolute()`. It
has no hex helper (22 float triples with hex comments), and `menuType`
(`:164-183`) is not a generic type helper — it carries a FOT-Rodin →
Avenir Next Condensed substitution, a `-0.08·size` tracking derivation
and `scaleX = 0.94f`, each annotated against the reference. Only
`smallType` (9 lines) would be replaced.

> **Combined: ~50 of 1,498 lines, 3.3%, and 0.7% on Persona alone.**

**What this changes.** Not the verdict — 389 quartets and nine
independent hand-rolls is still real, and `rect()` is a value spelling
rather than a design decision. What it changes is the *claim*. The
placement primitive is a **measured-reconstruction primitive**. It is for
the workflow where positions arrive as numbers off a reference plate. It
does close to nothing for a scene laid out in flex from a footprint
table, and that sentence ships in its doc comment so nobody reaches for
it in place of flex.

The prelude bundle is the item that survives this test intact, because
its evidence is 23 and 30 independent hand-rolls rather than a site
count — and it is why §7 ranks it above the placement primitive.

---

## 4. The proposed layer

Nothing new in `ElementNode`, nothing new in `PaintProps`, nothing that
participates in reconciler equality that does not already.

### 4.1 `Element::rect` / `Element::at` — kernel, `Compose.h`

```cpp
/** Place an absolute node on a parent-space RECT: the peer of centerAt(),
 *  for the case where you already know the box.
 *
 *  FOR MEASURED RECONSTRUCTION. Use this where coordinates arrive as
 *  numbers off a reference — nine study files hand-rolled it. A scene
 *  laid out in flex wants row()/column() and will gain almost nothing:
 *  measured, this saves 4.3% on ScenesInventory.h and 0% on
 *  ScenesPersona.h, whose nodes carry no explicit size at all. */
Element &rect(const SkRect &r);
/** Pin an absolute node's top-left to a parent-space POINT, leaving the
 *  node to size itself from its content. */
Element &at(SkPoint topLeft);
```

Companion free functions in `Util.h`: `util::centred(SkPoint, w, h)`,
`util::xywh(x, y, w, h)`.

Both write only `layout.absolute`, `layout.hasInsets`, `layout.insets`,
`layout.width`, `layout.height` — all present, all inside
`LayoutProps::operator==` (`ComposeInternal.h:25-41`, defaulted). **Zero
bytes added to `ElementNode`, zero reconciler risk.**

**Serves:** 32 of 35 sketches, 4 of 31 gallery files. **Saves:** ~1,680
lines. **Does not cover:** `right()`/`bottom()` pinning, percentage
insets, `autoDim()` sides, or any node without an explicit size — those
keep the longhand, correctly.

#### Before / after — `chaucer_astrolabe.cpp:1978-2007`, 30 lines

```cpp
  Element panel(float x, float y, float w, float h, const char *title,
                const char *sub) {
    auto g = box().absolute().left(0).top(0).width(kW).height(kH);
    g.child(box()
                .absolute()
                .left(x)
                .top(y)
                .width(w)
                .height(h)
                .fill(Fill::color(hex(0xe8dcc2, 0.62f)))
                .stroke(stroke(1.0f, Fill::color(hex(0x241c15, 0.24f)),
                               PathFormat::Align::Inner)));
    g.child(text(toU8(title), type(faceLimb, 15, kRubric, 1.9f))
                .absolute()
                .left(x + 16)
                .top(y + 11));
    if (sub && *sub)
      g.child(text(toU8(sub), type(faceItalic, 14, hex(0x6b5a44)))
                  .absolute()
                  .left(x + 16)
                  .top(y + 32)
                  .width(w - 32));
    g.child(box()
                .absolute()
                .left(x + 16)
                .top(y + 52)
                .width(w - 32)
                .height(1)
                .fill(Fill::color(hex(0x241c15, 0.28f))));
    return g;
  }
```

With `rect`/`at` — and with the five dead `.absolute()` calls from §0
already gone — 15 lines:

```cpp
  Element panel(SkRect p, const char *title, const char *sub) {
    auto g = box().rect(SkRect::MakeWH(kW, kH));
    g.child(box().rect(p)
                .fill(Fill::color(hex(0xe8dcc2, 0.62f)))
                .stroke(stroke(1.0f, Fill::color(hex(0x241c15, 0.24f)),
                               PathFormat::Align::Inner)));
    g.child(text(toU8(title), type(faceLimb, 15, kRubric, 1.9f))
                .at({p.fLeft + 16, p.fTop + 11}));
    if (sub && *sub)
      g.child(text(toU8(sub), type(faceItalic, 14, hex(0x6b5a44)))
                  .at({p.fLeft + 16, p.fTop + 32}).width(p.width() - 32));
    g.child(box().rect(SkRect::MakeXYWH(p.fLeft + 16, p.fTop + 52,
                                        p.width() - 32, 1))
                .fill(Fill::color(hex(0x241c15, 0.28f))));
    return g;
  }
```

The geometry is now one value the caller can compute, inset and reuse —
`familiesPanel()` at `:2010` immediately declares `px, py, pw, ph` as
four loose floats to feed it.

### 4.2 `<sigilcompose/Studio.h>` — the prelude, in the `util` tier

**The strongest-evidenced item here, and the only one both populations
demand independently** (23 `hex` hand-rolls, 30 `type` hand-rolls, ports
excluded).

```cpp
namespace sigil::compose::studio {

// ---- colour ----
constexpr SkColor4f hex(uint32_t rrggbb, float a = 1.0f);
constexpr SkColor4f alpha(SkColor4f c, float a);        // 45 gallery sites
constexpr SkColor4f scale(SkColor4f c, float k, float a = -1);  // 16
constexpr SkColor4f mix(SkColor4f a, SkColor4f b, float t);

// ---- type: a designated-init facade over weave::TextStyle ----
struct Type {
  sk_sp<SkTypeface> face;
  float size = 16;
  SkColor4f color = {0, 0, 0, 1};
  float track = 0;        // px of letter spacing
  float condense = 1;     // ShapingStyle::scaleX
  float weight = 0;       // 0 = none; else a "wght" variation
  bool aliased = false;   // hard-edged 1-bit rasterisation
  bool antiAlias = true;
};
sigil::weave::TextStyle type(const Type &);

// ---- faces, transitions, misc ----
sk_sp<SkTypeface> pickFace(std::initializer_list<const char *> families,
                           SkFontStyle style = SkFontStyle::Normal());
Transition ramp(std::chrono::milliseconds delay,
                std::chrono::milliseconds duration,
                choreograph::EaseFn ease = choreograph::easeOutQuad);
std::string fmt(const char *fmt, ...);
}
```

Plus one umbrella include (`<sigilsketch/Studio.h>`) so a file opens with
two lines instead of twenty (930 `#include` lines corpus-wide).

**Why `Type` is a struct and not a positional function — this is the
load-bearing design decision, and the corpus already ran the experiment.**
`GalleryCore.h:35` ships `styleAt(size, colour)`, two positional
parameters, and twelve scenes wrote their own anyway. A positional helper
cannot grow; a designated-init aggregate can, and
`Transition{.duration = …}` / `Grid{.columns = …}` already set that
precedent here.

**Expect three scenes to keep their own and call that success.**
`ScenesVeloren.h:106-125` carries a mandatory black underlay and explains
at `:118-124` why it is not optional; `ScenesPersona.h:164-183` carries a
typeface substitution and a derived tracking law;
`ScenesSkillTree.h:122-132` uses `slnt` rather than `wght`. A facade that
tried to absorb those would be the fourth failed header.

**Serves:** 29 of 35 sketches and 7 of the 12 independent gallery
headers. **Saves:** ~780 prelude + ~600 include + 49 fallback ≈ **1,430
lines**, and makes 33 hand-converted colour constants checkable.

**Does not cover, and must not try to:** palette values, family names,
sizes, durations. `studio::type` builds a `TextStyle` from numbers you
supply; it has no notion of "caption". `prog()` is **dropped** — §2.1.

### 4.3 `console::panel` and `console::monoStyle`

```cpp
struct Panel {
  std::vector<const LineRing *> rings;
  Style style;
  bool column = true;
  float padding = 10, gap = 6;
  Fill fill, border;                 // border painted Align::Inner
  float dividerAlpha = 0.15f;
};
Element panel(const Panel &);
Style monoStyle(sk_sp<SkTypeface> mono, float size, SkColor4f base,
                std::array<SkColor4f, 5> palette);   // dim/head/pass/num/fail
```

**Serves:** six independent sketches plus `ScenesConsole.h`. **Saves:**
~190 lines. **Does not cover:** a 2×2 grid of panels —
`sigillum_aemeth.cpp:1761-1773` nests two in a row, which is one line of
composition and should stay that way.

### 4.4 `debug::check` — the proving primitive

```cpp
struct Check { std::string label, expected, actual; bool pass = false; };
Check check(std::string label, long expected, long actual);
Check check(std::string label, double expected, double actual, double tol);
Check check(std::string label, std::string_view expected, std::string_view actual);
Check check(std::string label, bool condition);
void report(console::LineRing &, const Check &, int labelWidth = 44);
int failures(std::span<const Check>);      // for --verify exit codes
```

**Serves:** 5 sketches. **Saves:** ~130 lines. The real payoff is that a
study's claims become falsifiable rather than asserted —
`minard_1869.cpp:2580` already proves the shape works.

### 4.5 `compose::alphaMask` — the 1-bit bake

```cpp
struct Mask { sk_sp<SkImage> image; SkISize size; SkIRect ink; float advance; };
Mask alphaMask(Element tree, sigil::weave::FontContext &fonts,
               uint8_t threshold = 128, bool cropToInk = true);
```

**Serves:** 2 sketches (~50 of 179 lines). **Placement:** it is
`debug::rasterize` under a name that does not say "not for shipping art"
— either it lives beside it and `Debug.h` is renamed, or `rasterize`
moves.

**Does not close the actual hole.** Both studies wrote 100 lines because
`text()` takes no `PropValue` (§2.6). **File that separately in ROADMAP —
a mask helper will bury it.**

### 4.6 `shapes::chords(n, inset)` and `TextPath::atDeg`

```cpp
/** The n sides of a regular n-gon as n OPEN contours of one path, wound
 *  so glyph-up comes out radially outward. TextPath walks every contour
 *  as ONE arc-length coordinate, so side k's midpoint is at (k+0.5)/n. */
shapes::OutlineFn chords(int n, float inset = 0, float startDeg = -90,
                         SkPathDirection = SkPathDirection::kCW);
```

19 lines at `sigillum_aemeth.cpp:422`, one study; on the list only
because it is an exact peer of `shapes::polygon(n)` with a shorter
parameter list than body.

Separately, and worth more than any component here:

```cpp
struct TextPath {
  float at = 0.0f;                  // arc-length fraction
  std::optional<float> atDeg;       // …or degrees from the path's own
};                                  //   start, in its own winding sense
```

Not an extraction — the removal of a library convention leaking into user
arithmetic in files that compute angles hundreds of times.

### 4.7 What the layer does NOT cover — the contract

- **No placement policy.** Nothing decides where a thing goes. `rect()`
  takes the rect *you* computed. `Radial` already covers the case where a
  scheme is right (§1.1); `stickerScatter` shows what happens past it.
- **No palettes, no type scales, no theme.** A design-token layer would
  be the third failed header.
- **No widgets.** **Eight files across both populations wrote
  `Element label(…)` and no two agree what a label is**:
  `astral_tome.cpp:475` scrims it, `chevreul_circle.cpp:409` sizes it to
  1.7× the font, `black_watch.cpp:547` to 1.6×, `cde_motif.cpp:817` takes
  a `string_view` and a colour, `ScenesNetwork.h:87` is
  `.inset(x, y, 0, 0).zIndex(8)`, `ScenesY2k.h:203` has no placement at
  all. Eight spellings of one word is the signature of a *decision*.
  Compare `hex()`: three names, one body.
- **No animation grammar**, and specifically no phase/breath/ramp
  helpers until the clock is reachable (§2.4).
- **No `ElementNode` growth.**

---

## 5. What must NOT be extracted

The test: **does the component make a chore cheap, or a choice cheap?**
The corpus's own answer to what happens past that line is
`stickerScatter` (§1.2).

### 5.1 The lattice / node-table resolver — the strongest rejection

`game-RETRO.md:27-33` calls this *"the most repeated construction in the
corpus and the most obviously nameable."* Four refutations.

**(i) The bodies are shorter than the parameter list.**
`thaumonomicon.cpp:216` `centreOf` is 3 lines; `:228` `culled` is 5. A
component serving two studies needs origin, pitch, offset, cull rect,
cull *policy*, key index and a topology query — seven parameters for
eleven lines. And `:222-227` documents that the cull is applied to the
icon's top-left, drops a node more than half on screen, and deliberately
does **not** cull that node's edges, because the asymmetry is what makes
the browser read as a viewport onto a web that continues. The `+ 8` is
`GuiResearchBrowser:596-600`; the `- kLocX` is a scroll offset solved by
hand at `:210-212`. A component cannot hold those, and one that takes
them as arguments has abstracted nothing.

**(ii) The control group solves it offline and needs nothing.**
`gallery/SkillTreeData.h` is the other node web in the repo — a PoE
passive tree with the `orbitRadii`/`skillsPerOrbit` ladder, *exactly* the
lattice the retro describes — and `:25-32` stores `float x, y; ///< scene
space (900x640)`. The orbit resolution and the 0.34928 scale were applied
by `tools_gen_skill_tree_data.py` and frozen (`:6`, `:11-15`). **A
library resolver would have been code that scene deliberately did not
write.**

**(iii) Where a runtime web IS built, the shipped primitives cover it.**
`ScenesSkillTree.h:174` places a node as
`box().width(d).height(d).centerAt(at)`; `:492` routes every edge as
`rail({{nodeKey(e.a)}, {nodeKey(e.b)}}, router)`.
`thaumonomicon.cpp:1397` does the same through `connector()`.

**(iv) The extractable part is three lines, and it is a convention, not a
component.** `ScenesSkillTree.h:236`:

```cpp
static std::string nodeKey(int i) { return "n" + std::to_string(i); }
```

**The index into the table IS the element key**, and the derive phase
does the rest. That belongs in DOCUMENT's authoring guide as a named
idiom. `ScenesInventory.h` runs the identical grammar in different units
(`:281` → `cellX/cellY/spanW` `:308-310` → `:474`), and
`ScenesGerstner.h:91` in a third. **Every resolver differs because every
unit differs.**

**Verdict: reject. Document the index-as-key convention instead.**

### 5.2 The unit map / figure-local frame

`myst-RETRO.md:18-24` proposes `Frame{centre, radius, zeroAt, sense}` and
claims it "would delete most of those 135 absolutes". The 135 are not one
thing: 40 are `.absolute().inset(0)` full-bleed layer groups, which no
placement primitive addresses; the other 86 are rect placements that
`rect()` serves without a coordinate-system opinion.

The eight unit maps total ~20 lines and share no signature: `P(θ, rNorm)`
is polar with 0° at 12 o'clock; `vagrant_story_target.cpp:216-218`
carries two simultaneous snapping grids; `eva_magi_defense.cpp:244`
inverts a CRT roll; `thunder_fulu.cpp:547` is affine over a *polyline*.

And the independent gallery subset has **zero** unit maps — including the
ornament family, which is the gallery's own answer to "place by angle and
radius" and does it with `stack().width(2r).height(2r).centerAt(at)`
(`ScenesCosmati.h:224-233`, the `util::disc` shape).

One further data point: `ScenesBeethoven.h` genuinely needs
"(angle, normalised radius) against a figure-local frame" — `runs()` at
`:60-77` is a table of `{rInner, rOuter, startDeg, endDeg}` as plate
fractions — and resolves it in **ten lines** of `arcRun()` (`:107-141`),
reusing `trim()` for the reveal. It did not want a layout scheme; it
wanted an arc, and it got one. (Beethoven is a port, so this is
illustrative, not independent.)

**Verdict: reject the Frame. Ship `rect()`. Keep `P(θ, r)` as five lines
in the study that knows what θ means.**

### 5.3 The ring band of N cells

Verified against `sigillum_aemeth.cpp:720-791`. The 40 dividers are **one
node with forty contours and one trim window** (`trimStart 0.09`,
`trimEnd 0.91`). The 40 letters are **forty individually keyed nodes**,
each with its own radial fraction, seven carrying
`.opacity(with(0.30f, ramp(tDark*1000 + i*9, 700)))` because the
*solver* decided which cells the walk never visited. The numbers ride a
different radius per cell (`rNumOut` vs `rNumIn`).

A component owning the band owns all three, and the study's motion is
per-cell and derived from a solver the component cannot see.
`chaucer_astrolabe`'s limb (`:1580-1749`) shares the shape and none of
the parts.

### 5.4 A `ringLabel()` / `radialText()` widget

`TextPath` has **no `operator==`**, and the reconciler is conservative —
`Reconcile.cpp:151-160`: *"the baseline is an incomparable
std::function, so a run that has one never prunes"*, `if (ta.onPath)
return false;`. Every text-on-path node re-patches on every `render()`,
forever; `sigillum_aemeth` already mints ~150. A one-line widget makes it
cheap to mint hundreds more, invisibly. **Ship `atDeg` (§4.6), which
removes the arithmetic without making the nodes cheaper to mint.**

### 5.5 The leader-line callout

Two studies built it (`astral_tome.cpp:985-1028`, `vagrant_story_target`
header `:120`), and then Vagrant Story's implementer wrote the best
sentence in the corpus at `:135-137`:

> a column needs no leader, because a column IS the answer to "where does
> this readout go"

**Ship the 20-line recipe in the documentation with that sentence at the
top of it.** Not a component.

### 5.6 Palettes, data tables, and the icon-path vocabulary

Hex-literal counts: `xcom_battlescape` 256, `ksp_mapview` 214,
`chevreul_circle` 166, `chaucer_astrolabe` 138, `thaumonomicon` 134; the
gallery 178 plus 399 raw `SkColor4f{…}` literals. **These are the
cheapest lines in the corpus** — one constant, one hex, one provenance
comment (`ScenesCosmati.h:55` `// Mons Claudianus, purple`). They are the
research, written down.

The same holds for the shipped tables (`sigillum_aemeth`'s
`kRing`/`kNames`, `thunder_fulu`'s KanjiVG medians, `SkillTreeData.h`)
and for the **icon-path vocabulary** the gallery rebuilds:
`ScenesInventory.h:119-249` `artPath` (131 lines, 12 items) and
`ScenesVeloren.h:176-245` `glyphPath` (~70 lines, 8 icons), both
independent, both a switch over an enum returning hand-authored path
arithmetic. A library icon set would be a shipped opinion about what a
sword looks like. `ScenesInventory.h:117-118` already states the only
reusable idea — *author the path in the box you are handed* — and that is
a documentation sentence.

The asymmetry that makes this safe: **`hex()` is a component precisely
because it knows nothing about any palette.**

### 5.7 The legend row, the tick ladder, the reveal timeline, the state→visuals map

- **Legend row** — 5 gallery scenes (`ScenesCosmati.h:498-513`,
  `ScenesInventory.h:751-766`, `ScenesNetwork.h:113`, `ScenesSkillTree.h`,
  `ScenesBeethoven.h:196-207`), every one 8-16 lines with a different
  swatch: a stone sample, a colour chip, a stroked line, a typographic
  credit. The shape recurs; the substance does not.
- **Tick ladder** — already 12 lines with `ScatterBrush`
  (`astral_tome.cpp:757-777`) or 26 for 360 instanced ticks with three
  length classes (`chaucer_astrolabe.cpp:3132-3158`). **Zero** in the
  gallery. No two are the same ladder.
- **Reveal timeline** — 168 `withFrom(0,1,…)` sites in 17 sketches, and a
  `Timeline`/`cue` value saves **zero lines**: `reveal(cue(tTicks) +
  300ms + i*25ms, 400ms)` is a rename with a new type to learn. Ship
  `ramp()` (six byte-identical copies, 43 lines) and stop.
- **State enum → visual constants** — `thaumonomicon.cpp:430-440`,
  `ScenesInventory.h:88` `rarityColor`, `ScenesSkillTree.h:93/110/116`.
  Every one is a `switch` returning artefact data. A component here is a
  `std::map` with extra steps.

### 5.8 The inline `outline([](SkSize){…})` generators

145 sites, ~1,474 lines, both populations. Every one a different
silhouette: a torn square (`thaumonomicon.cpp:667`), a MAGI plate with a
stem (`eva_magi_defense.cpp:508`), a sheared CRT bar
(`lain_navi.cpp:370`), 12 item silhouettes (`ScenesInventory.h:119`).
`Shapes.h` already carries every silhouette that generalised; the
remainder is by definition the residue. Only `circleOutline()` and
`hline()` (`ScenesSkillTree.h:135-151`) are generic, and `shapes::`
already covers circles.

**The 1,474 lines are the escape hatch working.** (One real adjacent
defect, already `ROADMAP.md §3`: an `OutlineFn` is an incomparable
`std::function`, so `outline()` never prunes.)

### 5.9 The 12,566 comment lines

`thaumonomicon` 146 lines of header, `lain_navi` 270, `astral_tome` 284;
`SkillTreeData.h:1-16` is the gallery's model of the same voice — the
export it came from, the constants verbatim, the transform applied, and
"this is a study fixture, not a live import".

They are 19% of both populations and the reason either corpus is worth
anything. **Nothing here touches them.**

---

## 6. Constraints, checked against the source

1. **`sizeof(ElementNode) ≤ 768`** (`Composer.cpp:33`), `PaintProps`
   inline at 688 B. Every proposal either writes existing `LayoutProps`
   fields (`rect`/`at` — `ComposeInternal.h:25-41`) or is a free function
   outside the node. **No new node state, no `Box<>` block.**
2. **Live values must participate in reconciler equality.** `rect`/`at`
   write `LayoutProps`, whose `operator==` is defaulted, so they prune
   exactly as the longhand does. `studio::type` returns a
   `weave::TextStyle`, already compared by `textEqual`
   (`Reconcile.cpp:120-171`). Where this bites is §5.4 — text on a path
   never prunes (`Reconcile.cpp:151-160`) — which is why a `ringLabel()`
   widget is rejected.
3. **Read the source before believing it.** Claims that did **not**
   survive counting, mine included: "no gallery scene wrote a placement
   helper" (fourteen did, §2.3); "`Layouts.h` failed" (`Radial` works at
   both its call sites, §1.1); "the lattice resolver is the most nameable
   construction" (it is the least, §5.1); "135 hand-placed nodes" (86
   placements plus 40 layer groups, §5.2); `prog()` as a corpus pattern
   (2 gallery sites, §2.1). Claims that survived exactly: 23 independent
   `hex()` hand-rolls; 30 `type()`; 64 and 71 `.absolute()` in the two
   esoteric plates; `heptChords` at `sigillum_aemeth.cpp:422`; 53
   `fmt()` calls; two bitmap-font rasterisers; `stickerScatter` at zero
   users.
4. **Populations are not interchangeable.** Nine gallery headers are
   sketch ports and are excluded from every corroboration count. Twelve
   sketches share one standing brief, so their convergences have a common
   cause; §4.2 is the only proposal that does not rest on them.
5. **Perf.** Nothing proposed changes a paint path. Two warnings ship
   attached to the components that invite them:
   - **`.absolute().inset(0)` (293 sketch + 86 gallery sites) is a
     full-canvas node.** A binding on one is a `saveLayer` the size of the
     canvas — the measured 18.38 → 7.94 ms. See also
     `eva_magi_defense.cpp:919-923` for the opposite failure: `inset(0)`
     inside a `slot()` lays out 1920×0, "harmless for an outline in
     absolute coordinates, and a lie in every query".
   - **`alphaMask` bakes at 1× and blits `kNearest`.** It is not
     `bakeScale`.

---

## 7. Recommendation, ordered

Ordered by evidence strength and risk, not by line count.

| | item | evidence | saves | risk |
|---|---|---|---:|---|
| 0 | **Delete the dead `.absolute()` calls** | 1,316 of 1,379 (95.4%), verified `Compose.cpp:88-129`; both populations | **~1,316** | **none — mechanical, no behaviour change** |
| 1 | `<sigilcompose/Studio.h>` — `hex`, `Type{}`+`type`, `alpha`/`scale`, `pickFace`, `ramp`, `fmt` | **both populations, ports excluded**: 23 independent `hex` hand-rolls, 30 `type`, three names for one body | ~1,430 | none — free functions |
| 2 | `Element::rect(SkRect)` / `at(SkPoint)` | 32 of 35 sketches, 9 independent reinventions — **but 4.3% on Inventory and 0% on Persona; ship it scoped** | ~1,680 | none — existing fields, existing equality |
| 3 | `console::panel` + `console::monoStyle` | 6 independent sketches + `ScenesConsole.h` | ~190 | low |
| 4 | `debug::check` + `debug::report` | 5 sketches; `minard_1869.cpp:2580` already invented it | ~130 | low; the qualitative win exceeds the line win |
| 5 | `compose::alphaMask` (+ settle `Debug.h`'s name) | 2 sketches, 25-line shared core | ~50 | low |
| 6 | `shapes::chords(n, inset)` | 1 sketch, exact peer of `shapes::polygon` | ~19 | low |
| 7 | `TextPath::atDeg` | 8 sketches convert angle→fraction by hand | ~0 | none |

**File in ROADMAP, do not build here:** `Ticker::add`/`SketchContext`
cannot reach elapsed time, so **36 files keep a private copy of the
framework's clock** (§2.4); `text()` takes no `PropValue`, which cost two
studies 180 lines (§2.6); `LayoutScheme::place` returns rects, so
per-instance rotation and phase force hand-placement (§1.1); `outline()`
never prunes.

**Delete:** `layouts::stickerScatter` — zero users, and the one scene
that needs it refuses it in writing (§1.2).

**Do not build:** a frame/coordinate value, a lattice resolver, a
ring-band component, a `ringLabel` widget, `leaderTo`, a tick-ladder
component, a legend row, an icon set, a palette or theme layer, a
reveal-timeline DSL, a `label()` widget, a state-enum→visuals mapper,
`prog()`.

### What the merge changed

Reading the two inventories against each other moved five things, and all
five moved toward *less* framework:

1. **A pure deletion outranks the whole layer.** 1,316 dead `.absolute()`
   calls, zero risk, no design decision. It goes first.
2. **`Layouts.h` is not a failure**, and the real finding is narrower and
   more useful: a layout scheme returns rectangles, and real placement is
   (rect, rotation, phase). The failure to point at is `stickerScatter` —
   zero users, refused in writing, conceded in its own doc comment.
3. **Nine gallery headers are sketch ports.** Every corroboration count
   here is the re-counted one. Both headline claims survived (23 and 30
   independent hand-rolls); `prog()` and the bitmap rasteriser did not.
4. **The placement primitive is scoped, not universal.** 4.3% on
   Inventory, 0% on Persona, whose nodes carry no explicit size. It is a
   measured-reconstruction primitive and its doc comment says so.
5. **The prelude moved above it**, because it is the only item whose
   evidence is independent hand-rolls across two populations rather than
   a site count — and because the corpus already showed exactly how to
   get its signature wrong (`GalleryCore.h:35`).

**The honest summary.** Neither population is verbose because
SigilCompose is missing a framework. Both are verbose in three specific,
boring places — **a flag that sets itself**, **how a file starts**, and
**how a measured box is placed** — and everywhere else the length is
research provenance (12,566 comment lines), artefact arithmetic, or the
escape hatch doing its job. Items 0-2 are ~4,400 of the ~4,800
extractable lines and not one of them makes a design decision on the
author's behalf.

The framework this corpus wants is not Next.js to its React. It is a
`sed` script, a slightly larger `Util.h`, and one deletion.
