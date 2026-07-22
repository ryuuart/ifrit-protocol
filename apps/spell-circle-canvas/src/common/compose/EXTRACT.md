# EXTRACT — what every study built by hand, and what the layer above should be

Phase-2 inventory, `extract` stream. Read against **two populations**:

| | files | lines | comment | character |
|---|---:|---:|---:|---|
| sketches (`sketch/sketches/*.cpp`) | 33 | 49,892 | 10,110 | one artefact per file, one author, one sitting, all to a common brief |
| gallery (`gallery/*.h`, `*.cpp`) | 31 | 10,585 | 1,490 | older, pre-`--bench`, mixed subjects, the repo's house-style reference |
| **combined** | **64** | **60,477** | **11,600** | |

Plus `API.md`, `ROADMAP.md`, `Util.h`, `Layouts.h`, `Console.h`,
`Debug.h`, `GalleryCore.h`, and `Reconcile.cpp`/`Compose.cpp` for the
constraints. Every number below was produced by counting the source, not
by reading a report; where a retro's claim did not survive counting, that
is said so explicitly.

**Why the two populations are reported separately everywhere below.** The
sketches share a brief, so twelve agents converging on the same helper is
a *shared cause*, not independent evidence. The gallery is the control
group: different authors, different era, different subjects, no common
instruction set. A pattern present in both is real. A pattern present only
in the sketches is suspect. A pattern present only in the gallery may be
obsolete. Each finding below is graded on that axis.

No library code is proposed as written. This is the inventory and the
design. Nothing here has been built.

*Counts are a snapshot taken 2026-07-22 while `thunder_fulu`,
`bg3_dice_roll` and `eva_magi_interior` were still in flight; the two
that had landed are included, `eva_magi_interior` is not. No sketch was
edited.*

---

## 0. The one fact that should govern every decision in this document

**This library has already run the experiment twice, and the results are
opposite.**

`<sigilcompose/Util.h>` is the existing "layer above" — 206 lines of
deliberately-demoted sugar. Adoption across the study corpus:

| helper | uses | files |
|---|---:|---:|
| `util::toU8` | 387 | 28 |
| `util::stroke(w, fill, align)` | 166 | 21 |
| `util::disc(centre, r)` | 89 | 10 |
| `util::radialGradient` | 11 | 8 |
| `util::shadow` | 11 | 5 |

`<sigilcompose/Layouts.h>` is the *other* existing layer above — 307
lines, seven placement schemes (`Radial`, `AlongPath`, `ModularGrid`,
`Diagonal`, `StickerSlot`, `BaselineGrid`, `Scatter`), shipped as "this
codebase's native idiom (spell circles, ring labels)". Adoption:

```
$ grep -n 'layouts::' sketch/sketches/*.cpp
nightingale_coxcomb.cpp:288:  // layouts::AlongPath throws the tangent away (it passes nullptr to

$ grep -n 'layouts::' gallery/*.h
ScenesFlourish.h:161  ScenesFlourish.h:225  ScenesFlourish.h:323
ScenesOrganic.h:40    ScenesOrganic.h:71
```

**Zero uses in 33 studies. Five uses in the gallery, all in the two
abstract/decorative scenes, none in a reference-grounded one.** The single
sketch hit is a comment explaining why it was rejected.
`stroke_atlas.cpp:62` includes `Layouts.h` and uses nothing from it. The
`LayoutScheme` *seam* is used exactly once in either population
(`spacejam_1996.cpp:1337`, with a hand-written scheme). Every other
`layouts::` call site in the repository is in the library's own tests,
bench and demo.

That split is the finding, not the zero. `Layouts.h` was used by the
population written *alongside* it and by nobody who arrived afterwards
with a real artefact to reconstruct. The two studies whose *entire
subject* is non-rectangular placement — `sigillum_aemeth` and
`thunder_fulu` — called `Radial` zero times and hand-placed instead
(verified: 64 and 71 `.absolute()` blocks respectively).

The difference between the two headers is not quality, size, or
documentation. It is this:

> **`util::stroke` names a value spelling. `layouts::Radial` names a
> design decision.** Sugar over a spelling you were going to write
> anyway gets used 166 times. A component that decides *where things go*
> gets used zero times, because deciding where things go is what the
> author came here to do.

Every candidate below is sorted by which side of that line it falls on.

---

## 1. What recurs — measured

### 1.1 The placement longhand. 1,150 sites — but it is a SKETCH phenomenon.

Chain shapes following `.absolute()`, whitespace-insensitive match:

| chain | sketches | gallery |
|---|---:|---:|
| `.absolute().inset(…)` | 323 (293 of them `inset(0)`) | 169 |
| `.absolute().left().top().width().height()` | 287 | 29 |
| `.absolute().left().top()` | 154 | 33 |
| `.absolute().left().top().width()` | 60 | 0 |
| `.absolute().width().height()` | 36 | 0 |

Builder-call totals: sketches `.absolute(` 950, `.width(` 905, `.height(`
851, `.left(` 626, `.top(` 621. Gallery `.absolute(` 288, `.inset(` 222,
`.width(` 243, `.left(` 84, `.top(` 83.

**The ratio is the finding.** In the sketches, `.left(` outnumbers
`.inset(` almost 2:1 (626 : 328). In the gallery it is the reverse, 1:2.6
(84 : 222). The gallery is flex-native and the sketches are
coordinate-native, and the reason is not style: **a study reconstructs an
artefact by measuring pixel coordinates off a reference plate, so its
positions arrive as numbers, not as relationships.** `chaucer_astrolabe`
carries 53 LTWH quartets and every one of them is a measurement.

So `rect()` is not a universal ergonomics fix. It is the right primitive
for the measured-reconstruction workflow specifically — which is what the
study program is, and what the gallery mostly is not. That qualification
belongs in its doc comment.

**`.absolute()` before `.left()`/`.top()` is redundant** — verified at
`Compose.cpp:101-124`, every edge setter sets `layout.absolute = true`
itself. 638 of the 950 `.absolute()` calls in the corpus are dead
characters.

**Nine studies wrote the same three-line helper**, under four names, in
two shapes:

```
black_watch.cpp:544        inline Element at(float x, float y, float w, float h)
chevreul_circle.cpp:406    inline Element at(float x, float y, float w, float h)   // byte-identical
xcom_battlescape.cpp:842   inline Element at(float x, float y, float w, float h)
spacejam_1996.cpp:244      inline Element rect(float x, float y, float w, float h)
fallout2_charsheet.cpp:384 inline Element at(Element e, float x, float y, float w, float h)
winamp_base.cpp:248        inline Element at(Element e, float x, float y, float w, float h)
ksp_mapview.cpp:237        inline Element place(Element e, float x, float y, float w, float h)
ksp_mapview.cpp:242        inline Element at(Element e, SkPoint c, float w, float h)
twoadvanced_v4.cpp:410     inline Element place(Element e, float x, float y, float w, float h)
```

`black_watch.cpp:544` and `chevreul_circle.cpp:406` are the same line
twice:

```cpp
inline Element at(float x, float y, float w, float h) {
  return box().absolute().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
}
```

And the studies that did **not** write the helper are exactly the ones
carrying the longhand: `chaucer_astrolabe` 53 quartets, `ds2_bench` 26,
`astral_tome` 23, `bg3_dice_roll` 21, `thunder_fulu` 20,
`vagrant_story_target` 18 — against `black_watch` 1, `chevreul_circle` 2,
`winamp_base` 2, `spacejam_1996` 3.

**No gallery scene wrote one** (0 of 31), which is consistent with the
ratio above: the gallery does not have enough measured rects to feel the
loss.

### 1.2 The prelude. 820 lines, 45 of 64 files — and it is present in BOTH populations.

This is the one candidate where the control group makes the case rather
than weakening it. Measured by brace-matched span:

| helper | sketch defs | gallery defs | total lines |
|---|---:|---:|---:|
| `type(…) -> weave::TextStyle` | 23 | 18 | 430 |
| `face(family, …) -> sk_sp<SkTypeface>` (fallback chain) | 15 | 1 | 168 |
| `hex` / `rgb` / `C` `(0xRRGGBB, a)` | 19 | 5 | 101 |
| `fmt(const char*, …) -> std::string` | 7 | 0 | 53 |
| `ramp(delay, dur, ease) -> Transition` | 6 | 0 | 43 |
| `mul` / `dim` / `mix` / `fade` / `alpha` colour ops | 11 | 0 | 22 |
| `prog(PaintProgram) -> Decoration` | 3 | 0 | 3 |
| **total** | **84** | **24** | **820** |

Plus 49 lines of `if (!faceX) faceX = …` fallback chains (sketches), and
930 `#include` lines (643 + 287; median 20 per sketch,
`slitscan_2001.cpp` has 37).

**The colour helper is the decisive one.** The gallery defines *no*
`hex()` — and then five separate headers define the identical four lines
under the name `C`:

```
ScenesCosmati.h:48   ScenesGerstner.h:51   ScenesInventory.h:69
ScenesVeloren.h:55   ScenesY2k.h:51
constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f, a};
}
```

Twenty-four definitions of one function across two populations, three
eras and three names (`hex` 12, `rgb` 7, `C` 5), with no shared brief
between the groups. That is not convergence caused by a common
instruction set. **That is a missing primitive.**

The `ramp()` bodies are byte-identical seven lines in six sketches
(`chaucer_astrolabe.cpp:601`, `sigillum_aemeth.cpp:521`,
`minard_1869.cpp:832`, `vertigo_titles.cpp:291`,
`nightingale_coxcomb.cpp:244`, `chladni_tab1.cpp:380`) and appear nowhere
in the gallery — grade that one sketch-only.

`type()` varies only by *addition* over an identical six-statement core —
`lain_navi.cpp:588` adds a mask-filter blur and `kPlus`,
`vagrant_story_target.cpp:1209` adds `aliased`, `twoadvanced_v4.cpp:168`
reads tracking in per-mille, `ScenesInventory.h:99` adds a `wght`
variation:

```cpp
s.shaping.typeface = tf;  s.shaping.fontSize = size;
s.shaping.letterSpacing = track;  s.shaping.scaleX = condense;
s.paint.foreground.setColor4f(c, nullptr);
s.paint.foreground.setAntiAlias(aa);
```

**And the gallery already tried to ship this one, and it failed by being
too small.** `GalleryCore.h:35`:

```cpp
inline sigil::weave::TextStyle styleAt(float size, SkColor color = SK_ColorWHITE) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.paint.foreground.setColor(color);
  return s;
}
```

Two parameters, no face, no tracking, no antialias. Sixteen of the
gallery's own scene headers wrote their own `type()` anyway. That is a
direct instruction about the parameter set: a positional two-argument
helper is not enough, and a positional seven-argument one is unusable.
**A designated-init struct is the shape that survives** — it can grow
without breaking a call site, which is exactly why `styleAt` could not.

### 1.3 The verification console. 7 files, both populations, ~260 lines of chrome.

`console::LineRing` + `console::Style` + a hand-built panel:
`chaucer_astrolabe.cpp:2507-2546` (4 panels, 2×2),
`sigillum_aemeth.cpp:1719-1775` (4, 2×2),
`thunder_fulu.cpp:1494-1533` (3, column),
`minard_1869.cpp:2531-2560` (5, row),
`psx_doom_fire.cpp:565-605`, `eva_magi_defense.cpp:1044-1065`, and — in
the control group — `gallery/ScenesConsole.h:122` (`style()`) + `:205`
(the panel). Seven independent panels, no shared brief between the
sketch six and the gallery one.

Every one of them: a mono `TextStyle` repeated 5× for the palette, an
absolutely-placed panel with a fill and an inner stroke, an inner padding
box, and dividers spelled as 1-px `box().fill(…)` children.

117 `append(toU8(…))` calls total; 21 of them carry a conditional palette
index — i.e. they are **checks**. `minard_1869.cpp:2580` independently
invented the missing primitive as a lambda:

```cpp
auto chk = [&](console::LineRing &r, const std::string &label, long lhs, long rhs) {
  char buf[160];
  std::snprintf(buf, sizeof buf, "  %-44s %8ld   %s", label.c_str(), lhs,
                lhs == rhs ? "EXACT" : "DIFF");
  r.append(toU8(buf), lhs == rhs ? 1 : 2);
};
```

`sigillum_aemeth` and `thunder_fulu` between them make 53 `fmt()` calls
(14 + 39) producing the same shape by hand.

### 1.4 The 1-bit mask bake. Two studies, ~180 lines, one shared core.

`thaumonomicon.cpp:1134-1205` (`PixText` / `bakeText` / `blitText`, 72
lines) and `vagrant_story_target.cpp:414-520` (`Cell` / `PixFont` /
`bakeCell` / `bakeFont` / `blit` / `widthOf`, 107 lines).

The shared core is 25 lines in each, and is the *same sequence*:

```
measure(box().child(text(u8, st)), fonts)
  → SkSurfaces::Raster(N32) → snapshot(tree, fonts) → drawPicture
  → readPixels(RGBA8888 unpremul) → threshold alpha → SkBitmap A8 → asImage
```

The reason both exist is stated at `vagrant_story_target.cpp:409`:

> LIVE numbers (the hit percentage, RISK, the rate) cannot [use `text()`]:
> `text()` takes no `PropValue`.

Verified: `Compose.h:1204` is `Element text(std::u8string, TextStyle)`.

**And the shared core now exists in the library**, added this run:
`debug::rasterize(Element, FontContext&, SkISize, colorType, background)`
at `Debug.h:315`. Its own doc-comment says *"Three studies hand-rolled the
same forty lines"*. Neither bitmap-font study could use it — both predate
it, and it lives in a header called `Debug.h`.

### 1.5 Radial text. 24 sites, 8 files, 155 lines of `TextPath{…}` alone.

The block, spelled the same way in `sigillum_aemeth.cpp:756-788` and
`chaucer_astrolabe.cpp:1661-1675`:

```cpp
text(toU8(str), style)
    .absolute().width(Dim(2*r)).height(Dim(2*r)).centerAt(centre).key(k)
    .onPath(TextPath{.path = shapes::circle(), .at = f,
                     .align = TextPath::Align::Center, .offset = 0.0f,
                     .autoFlip = false, .orient = TextPath::Orient::Radial})
```

`.onPath(TextPath{` sites: `sigillum_aemeth` 10, `ksp_mapview` 5,
`chaucer_astrolabe` 4, `chevreul_circle` 2, plus one each in `lain_navi`,
`minard_1869`, `genesis_fire`, `vagrant_story_target`.

The friction is not the block. It is `f`. `sigillum_aemeth.cpp:279`:

```cpp
// θ → the arc-length fraction of shapes::circle(), whose contour starts at
// due EAST and runs clockwise (SkPathBuilder::addOval, startIndex 1, kCW).
float frac(float thDeg) { return std::fmod((thDeg - 90.0f) / 360.0f + 4.0f, 1.0f); }
float skAngle(float thDeg) { return thDeg - 90.0f; }
```

Three angle conventions in one file — the figure's, Skia's, and
arc-length fraction — converted by hand at every one of hundreds of call
sites. `shapes::circle(direction, startIndex)` exists (`Shapes.h`) and
does not remove the conversion, because `TextPath::at` is a *fraction*,
not an angle.

### 1.6 The unit map. 8 studies, ~20 lines total.

```
thaumonomicon.cpp:198    constexpr float g(float v) { return v * U; }
thaumonomicon.cpp:216    inline SkPoint centreOf(int col, int row)
astral_tome.cpp:326-328  g / gx / gy  (scale + page origin)
vagrant_story_target.cpp:216-218  snapG / snapX / snapY  (TWO grids: 4 px and 2.5 px)
sigillum_aemeth.cpp:273  SkPoint P(float thDeg, float rNorm)   (polar, 0° at 12 o'clock)
thunder_fulu.cpp:547     Poly place(const Poly&, SkPoint origin, float sx, float sy)
chaucer_astrolabe.cpp:194 SkPoint MC(float mx, float my)
chladni_tab1.cpp:206     SkPoint centreOf(const FigSpec&)
eva_magi_defense.cpp:244 SkPoint unroll(SkPoint) + mirrorX/mirrorP
```

Both retros nominate this as a component. **It is 20 lines of code in
total across eight studies**, no two of which have the same signature —
polar in one, two simultaneous snapping grids in another, an inverse CRT
roll rotation in a third. See §5.2.

**The gallery has none.** Zero unit maps in 31 files. Its node web
(`SkillTreeData.h`) stores coordinates pre-resolved — see §1.7.

### 1.7 What the control group does differently, and what that teaches

Three divergences worth acting on.

**(a) The gallery resolves coordinates offline.** `SkillTreeData.h:25-32`:

```cpp
struct Node {
  float x, y;        ///< scene space (900x640)
  uint8_t group;     ///< index into kGroups
  uint8_t orbit;     ///< index into the orbitRadii ladder
  Kind kind;  State state;  const char *name;
};
```

The header comment (`:11-15`) says the transform — "a uniform scale
(0.34928) so the orbit proportions survive" — was applied **when the data
was generated, then frozen**. There is no `centreOf()`, no lattice
resolver, no runtime coordinate map. `thaumonomicon.cpp:216` computes
`centreOf(col, row)` at runtime only because the artefact's own source
indexes by `col * 24`.

Two populations, the same problem, and the gallery's answer needs *no
library support at all*: pre-resolve into the table. That is a direct
counter-argument to extracting a lattice resolver (§5.1).

**(b) The gallery's node web needs nothing new.** `ScenesSkillTree.h`
builds 39 nodes and 60+ edges out of the shipped primitives and no
helpers: `socket()` at `:174` is `box().width(d).height(d).centerAt(at)`,
and every edge is `rail({{nodeKey(a)}, {nodeKey(b)}}, routers::orbit(…))`
at `:483-496`. `thaumonomicon.cpp:1397` reaches the same shape via
`connector(childKey, parentKey, thaumRoute(flipped))`. **The construction
the game retro calls "the most repeated in the corpus" is already covered
by `centerAt` plus the derive phase**, in both populations, and what
remains at each site is per-artefact arithmetic.

**(c) The gallery already ran this exercise once, at the right altitude.**
`OrnamentKit.h` (451 lines) is consumed by 4 scenes; `FlourishKit.h`
(164) by 2. Both are *domain vocabularies* — an ornament grammar, a
flourish grammar — not framework. Neither tries to place anything. That
is the precedent for what a successful extraction looks like here, and it
is the same shape as `util::stroke`: a value spelling, not a decision.

---

## 2. Component or coincidence — the verdicts

The test applied throughout: **is the parameter list shorter than the
body, and is there one obviously right behaviour?** A three-line function
that needs three parameters is a rename, not an abstraction.

"Both" in the population column means the pattern appears independently
in the sketches *and* in the gallery — different authors, no shared
brief. That is the strongest evidence available here.

| # | construction | population | verdict | why |
|---|---|---|---|---|
| 1 | rect/point placement | sketches (gallery is flex-native) | **component** | 3-line body, 1 parameter (`SkRect`). Nine independent reinventions. Writes only existing `LayoutProps` fields. Ships with the §1.1 qualification. |
| 2 | `hex()` colour conversion | **both** (19 + 5, three names) | **component** | 24 definitions of four identical lines, across two eras with no shared brief. |
| 3 | `type()` → `weave::TextStyle` | **both** (23 + 18) | **component** | 41 definitions of one six-statement core. `GalleryCore.h:35` proves the parameter set must be a struct, not positional. |
| 4 | `face()` fallback chain, `ramp`, `fmt`, `prog`, colour ops | mostly sketches | **component** | Zero decisions, byte-identical bodies. Ships with 2 and 3. |
| 5 | `console::panel(logs, style, …)` + `console::monoStyle` | **both** (6 + 1) | **component** | One right layout (a bordered box of stacked rings with dividers). Two parameters that matter: count and axis. |
| 6 | `debug::check(label, expected, actual)` | sketches | **component** | Turns a sentence-that-happens-to-be-true into a value. One behaviour. |
| 7 | `alphaMask(Element, fonts, threshold)` | sketches (2) | **component** | The 25-line core of two 100-line hand-rolls. `debug::rasterize` is already 80% of it. |
| 8 | `shapes::chords(n, inset)` | sketches (1) | **component** | 19 lines at `sigillum_aemeth.cpp:422`, exact peer of `shapes::polygon(n)`, one parameter set, one right winding. |
| 9 | radial ring text | sketches | **component, narrowly** | Only as a *fraction-from-angle* fix, not as a `ringLabel()` widget. See §4.6 and §5.4. |
| 10 | tick/division ladder | sketches | **reject** | Already 12 lines with `ScatterBrush` (`astral_tome.cpp:757-777`) or 26 for 360 instanced ticks with three lengths and three alphas (`chaucer_astrolabe.cpp:3132-3158`). No two are the same ladder. |
| 11 | lattice / node-table resolver | sketches only; **the gallery solves it offline** | **reject** | See §5.1. This is the strongest argument in the document. |
| 12 | unit map / figure-local frame | sketches only (8 defs, 20 lines); **gallery has none** | **reject** | See §5.2. |
| 13 | ring band of N cells | sketches | **reject** | See §5.3. Owning the band owns the per-cell animation. |
| 14 | leader-line callout | sketches | **reject as a component, ship as a recipe** | See §5.5. |
| 15 | palette / data tables | **both** | **reject** | They *are* the artefact. See §5.6. |
| 16 | one component instanced N times with a per-instance rotation | **both** | **reject** | It is a `for` loop calling a function (`eva_magi_defense.cpp:947-950`, `ScenesSkillTree.h:679`). There is nothing between the loop and the function to name. |
| 17 | reveal timeline (`withFrom(0,1,ramp(tX*1000+i*k, d))`) | sketches | **reject as a DSL, keep `ramp`** | 168 sites in 17 files, and a `Timeline`/`cue` value saves **zero lines**. See §5.7. |
| 18 | state enum → row of visual constants | **both** | **reject** | `thaumonomicon.cpp:430-440` (`tierMul`/`tierTint`/`tierZ`), `ScenesInventory.h:88` (`rarityColor`), `ScenesSkillTree.h:93/110/116`. Every one is a `switch` returning artefact data. A component here is a `std::map` with extra steps. |

---

## 3. Where the verbosity is, measured

Combined corpus: **64 files, 60,477 lines**, of which 11,600 are comment
(19%) and 3,950 are the per-study research header before the first
`#include`.

| idiom | sketch sites | gallery sites | lines it costs | lines a fix saves |
|---|---:|---:|---:|---:|
| `.absolute().left().top().width().height()` | 287 | 29 | ~1,580 | **~1,260** |
| `.absolute().left().top()` | 154 | 33 | ~560 | **~370** |
| the prelude (`hex`/`type`/`face`/`fmt`/`ramp`/`mul`/`prog`) | 84 defs | 24 defs | 820 | **~780** |
| `#include` blocks | 643 | 287 | 930 | **~600** |
| `.absolute().inset(0)` | 293 | 86 | 379 | **~379** |
| redundant `.absolute()` before an edge setter | 638 | 78 | 716 | (subsumed above) |
| console panel + `logStyle()` | 6 | 1 | ~260 | **~190** |
| check-printing (`fmt` + conditional palette) | ~74 | 0 | ~200 | **~130** |
| font fallback chains (`if (!faceX)`) | 49 | 0 | 49 | **~45** |
| 1-bit mask bake | 2 | 0 | 179 | **~50** |
| inline `outline([](SkSize){…})` generators | 127 | 18 | 1,474 | 0 — see §5.8 |
| raw `PaintProgram` lambdas | 67 | 12 | 2,025 | 0 — the escape hatch working |

Top offenders per study, LTWH quartets: `chaucer_astrolabe` 53,
`ds2_bench` 26, `astral_tome` 23, `bg3_dice_roll` 21, `thunder_fulu` 20,
`vagrant_story_target` 18, `twoadvanced_v4` 17, `stroke_atlas` 15,
`hitman_verlet` 12, `sigillum_aemeth` 12, `cde_motif` 11, `minard_1869`
11; the gallery's worst is `ScenesInventory.h` at 17 `.left(` sites.

Prelude cost per file: `lain_navi` 35, `chaucer_astrolabe` 29,
`sigillum_aemeth` 29, `fallout2_charsheet` 28, `eva_magi_defense` 27,
`winamp_base` 27, `minard_1869` 26, `twoadvanced_v4` 26. The gallery's
232 lines are 24 definitions spread over 16 of its 31 files, averaging
about 10 lines each — smaller per file, and just as unanimous.

**Total realistic saving: ~3,750 lines, 6.2% of the combined corpus.**
That number is deliberately unglamorous. Neither population is bloated
with framework-shaped repetition; both are ~19% research comment and
~35% artefact-specific arithmetic, and both of those are the point of the
exercise. What repetition exists is concentrated in *two* places — how a
box is placed and how a file starts — and both are pure spelling.

**And the split between the populations is itself the design brief.** The
prelude (row 3) is the only item where the gallery contributes a
comparable share of the evidence, and it is the only item where the
recurrence has no common cause. The placement rows are almost entirely a
sketch phenomenon. Ship both, but do not claim the second is universal.

---

## 4. The proposed layer

Two headers. Nothing new in `ElementNode`, nothing new in `PaintProps`,
nothing that participates in reconciler equality that does not already.

### 4.1 `Element::rect` / `Element::at` — kernel, `Compose.h`

```cpp
/** Place an absolute node on a parent-space RECT: the peer of centerAt(),
 *  for the case where you know the box. Implies absolute(); sets the same
 *  four LayoutProps fields the longhand does, so it prunes identically. */
Element &rect(const SkRect &r);
/** Pin an absolute node's top-left to a parent-space POINT, leaving the
 *  node to size itself from its content. */
Element &at(SkPoint topLeft);
```

Both write only `layout.absolute`, `layout.hasInsets`, `layout.insets`,
`layout.width`, `layout.height` — all present today, all inside
`LayoutProps::operator==` (`ComposeInternal.h:25-41`, defaulted). **Zero
bytes added to `ElementNode`, zero reconciler risk.**

Companion free functions, `Util.h`:

```cpp
namespace util {
inline SkRect centred(SkPoint c, float w, float h);   // the 15 hand-written
inline SkRect centred(SkPoint c, SkSize s);           // `x - w*0.5f` sites
inline SkRect xywh(float x, float y, float w, float h);
}
```

**Serves:** 30 of 33 studies, 6 of 31 gallery files. **Saves:** ~1,630
lines and nine duplicate helper definitions. **Does not cover:**
`right()`/`bottom()` pinning (12 + 11 sites), percentage insets,
`autoDim()` sides. Those keep the longhand, correctly — they are
different intents.

**Ship it with the §1.1 qualification in the doc comment**, because the
control group says it is not a universal need: the gallery uses `.inset(`
2.6× more than `.left(` and would gain ~180 lines from this, against the
sketches' ~1,450. `rect()` is for the workflow where positions are
*measured*, and that is worth saying at the call site so nobody reaches
for it in place of flex.

#### Before / after, real code — `chaucer_astrolabe.cpp:1978-2007`

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

30 lines. With `rect`/`at`:

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

15 lines, and the geometry is now one value the caller can compute,
inset, and reuse — `familiesPanel()` at `:2010` immediately declares
`px, py, pw, ph` as four loose floats to feed it.

### 4.2 `<sigilcompose/Studio.h>` — the prelude, in the `util` tier

**The strongest-evidenced item in this document**, and the only one both
populations independently demand. Not a framework: seven functions and
one struct, all of which a user could write, which is the `Util.h`
charter verbatim. Include it and the first 30 lines of every study file
and the first 20 of every gallery header disappear.

```cpp
namespace sigil::compose::studio {

// ---- colour: every source palette is a hex integer ----
constexpr SkColor4f hex(uint32_t rrggbb, float a = 1.0f);
constexpr SkColor4f alpha(SkColor4f c, float a);           // replace alpha
constexpr SkColor4f mul(SkColor4f c, float k, float a = -1);  // brightness ladder
constexpr SkColor4f mix(SkColor4f a, SkColor4f b, float t);

// ---- type: a designated-init facade over weave::TextStyle ----
struct Type {
  sk_sp<SkTypeface> face;
  float size = 16;
  SkColor4f color = {0, 0, 0, 1};
  float track = 0;        // px of letter spacing
  float condense = 1;     // ShapingStyle::scaleX
  bool aliased = false;   // hard-edged 1-bit rasterisation
  bool antiAlias = true;
};
sigil::weave::TextStyle type(const Type &);

// ---- faces: the fallback chain, once ----
sk_sp<SkTypeface> pickFace(std::initializer_list<const char *> families,
                           SkFontStyle style = SkFontStyle::Normal());

// ---- transitions ----
Transition ramp(std::chrono::milliseconds delay,
                std::chrono::milliseconds duration,
                choreograph::EaseFn ease = choreograph::easeOutQuad);

// ---- misc ----
std::string fmt(const char *fmt, ...);              // the 7 re-declarations
inline Decoration prog(PaintProgram p);             // the 3 re-declarations
}
```

Plus one umbrella include so a study opens with two lines instead of
twenty:

```cpp
// <sigilsketch/Studio.h>: Sketch.h + Compose/Shapes/Material/Patterns/
// Lines/Brushes/Decorations/Util/Studio + the eight Skia headers every
// study pulls in anyway. Nothing here is required; every study that
// wants a smaller surface keeps its own list.
```

**Serves:** 29 of 33 studies **and 16 of 31 gallery files** — the only
item in this document with that property. **Saves:** ~780 prelude lines
+ ~600 include lines + 49 fallback lines ≈ **1,430 lines**.

**Why `Type` is a struct and not a positional function.** `GalleryCore.h:35`
already ships `styleAt(size, colour)` — two positional parameters — and
sixteen of the gallery's own scene headers wrote their own `type()`
anyway, because they needed a face, or tracking, or a `wght` variation
(`ScenesInventory.h:99`), or condensation. A positional helper cannot
grow; a designated-init aggregate can, and `Transition{.duration = …}`
and `Grid{.columns = …}` already set that precedent in this codebase.

**Does not cover, and must not try to:** the palette *values*, the family
*names*, the sizes, the durations. `studio::type` takes a face and
returns a `TextStyle`; it does not know what a caption is. `pickFace`
takes a list and returns the first that resolves; it does not know that
Herculanum should fall back to Optima. Studies that add a mask filter or
a blend mode to their `type()` — `lain_navi.cpp:588` — keep doing that on
the returned value, which is already how they do it.

**Naming note.** `studio::hex` collides with nothing. Existing spellings:
`hex` in 12 sketches, `rgb` in 7, `C` in 5 gallery headers. Pick `hex` —
`rgb(0xRRGGBB)` reads as "three channels" when the argument is one
integer, and `C` is unsearchable.

### 4.3 `console::panel` and `console::monoStyle`

```cpp
namespace console {
/** A bordered console plate: N LineRings stacked on `axis` with hairline
 *  dividers between them, an inner padding, a fill and an inset stroke.
 *  The six hand-built panels differ only in count, axis and colour. */
struct Panel {
  std::vector<const LineRing *> rings;
  Style style;
  bool column = true;
  float padding = 10, gap = 6;
  Fill fill, border;      // border painted PathFormat::Align::Inner
  float dividerAlpha = 0.15f;
};
Element panel(const Panel &);

/** The mono style every study built by hand: one face, one size, and the
 *  five-entry palette {dim, heading, pass, number, fail}. */
Style monoStyle(sk_sp<SkTypeface> mono, float size, SkColor4f base,
                std::array<SkColor4f, 5> palette);
}
```

**Serves:** `chaucer_astrolabe`, `sigillum_aemeth`, `thunder_fulu`,
`minard_1869`, `psx_doom_fire`, `eva_magi_defense`, and the control
group's `gallery/ScenesConsole.h`. **Saves:** ~190 lines.
**Does not cover:** a 2×2 grid of panels — `sigillum_aemeth.cpp:1761-1773`
nests two `panel()`s in a row, which is one line of composition and
should stay that way.

### 4.4 `debug::check` — the proving primitive

Both proving plates prove themselves on screen and neither can be
falsified by its own output. `sigillum_aemeth` calls `debug::coverage`
twice (`:1947`, `:1983`) and then hand-formats the verdict;
`thunder_fulu` calls no `debug::` at all. Between them, 53 `fmt()` calls
producing strings whose truth is not connected to the assertion.

```cpp
namespace debug {
struct Check {
  std::string label;
  std::string expected, actual;   // formatted
  bool pass = false;
};
Check check(std::string label, long expected, long actual);
Check check(std::string label, double expected, double actual, double tol);
Check check(std::string label, std::string_view expected, std::string_view actual);
Check check(std::string label, bool condition);          // the bare assertion

/** Append to a console ring, formatted in columns, palette index chosen
 *  by pass/fail. The half that was missing: the library supplied the
 *  measurement and nothing supplied the reporting. */
void report(console::LineRing &, const Check &, int labelWidth = 44);

/** Non-zero if any check failed — for `--verify` exit codes. */
int failures(std::span<const Check>);
}
```

**Serves:** 5 studies immediately, and every study written afterwards.
**Saves:** ~130 lines. **The real payoff is not lines** — it is that a
study's claims become machine-checkable, and that a failed check is
visible on the plate instead of being a sentence that reads fine.
`minard_1869.cpp:2580` already proves the shape works.

### 4.5 `compose::alphaMask` — the 1-bit bake

```cpp
/** Rasterise an element tree and threshold it into a 1-bit A8 mask,
 *  cropped to its ink. THE bitmap-font substitute: SigilWeave has no
 *  bitmap-font path, and an aliased shaping at 1× read back through a
 *  threshold is one. Returns the mask, its ink size, and the advance the
 *  laid-out tree measured. */
struct Mask { sk_sp<SkImage> image; SkISize size; SkIRect ink; float advance; };
Mask alphaMask(Element tree, sigil::weave::FontContext &fonts,
               uint8_t threshold = 128, bool cropToInk = true);
```

**Serves:** `thaumonomicon` and `vagrant_story_target` today (~50 lines
saved of 179), and — more importantly — the next study that needs a live
numeric readout, which is what drove both of them
(`vagrant_story_target.cpp:409`).

**Does not cover, and this is the real gap:** the *reason* both studies
wrote 100 lines is that `text()` takes a `std::u8string`, not a
`PropValue`. A mask helper makes the workaround cheaper; it does not
close the hole. **Recommend filing that separately as a ROADMAP item —
"a text leaf whose content is a bound value" — because it is a two-study
citation and a mask helper will bury it.**

**Placement note:** `debug::rasterize` (`Debug.h:315`) is the same
machinery under a name that says "not for shipping art". Either
`alphaMask` lives beside it and `Debug.h` gets renamed, or `rasterize`
moves. Two studies could have used it and neither knew it existed —
which is the corpus's most-repeated failure mode (`ROADMAP.md:74-92`,
"four entries were WRONG").

### 4.6 `shapes::chords(n, inset)` and the angle convention

```cpp
/** The n sides of a regular n-gon as n OPEN contours of one path, wound
 *  so glyph-up comes out radially outward. TextPath walks every contour
 *  in order as ONE arc-length coordinate, so side k's midpoint is at
 *  exactly (k + 0.5)/n. The primitive behind "type set along the sides
 *  of a polygon" — a heptagon of Names, a hexagonal cartouche, a
 *  pentagon of planets. */
shapes::OutlineFn chords(int n, float inset = 0, float startDeg = -90,
                         SkPathDirection = SkPathDirection::kCW);
```

19 lines at `sigillum_aemeth.cpp:422`, one study, and the only reason it
is on this list is that it is an exact peer of `shapes::polygon(n)` and
its parameter list is shorter than its body.

Separately, and **more valuable than any component in this document**:

```cpp
// TextPath::at is a FRACTION of arc length. Every radial study converts
// its own angle to one by hand; sigillum_aemeth.cpp:279 is the scar.
struct TextPath {
  …
  float at = 0.0f;
  /** Alternative to `at`: degrees measured from the path's own start,
   *  in its own winding sense. On a shapes::circle() baseline this is
   *  the figure's angle, converted once, here, correctly. */
  std::optional<float> atDeg;
};
```

That is not an extraction. It is the removal of a library convention
leaking into user arithmetic in files that compute angles hundreds of
times, and it belongs in the code review as much as here.

### 4.7 What the layer does NOT cover — stated as a contract

- **No placement policy.** Nothing decides where a thing goes. `rect()`
  takes the rect *you* computed. This is the `Layouts.h` lesson (§0) and
  it is non-negotiable.
- **No palettes, no type scales, no theme.** `studio::type` builds a
  `TextStyle` from four numbers you supply; it has no notion of "caption"
  or "display". A design-token layer would be the third failed header.
- **No widgets.** No `panel()` that draws a titled frame, no `label()`,
  no `card()`. **Eight files across both populations wrote
  `Element label(…)` and no two agree on what a label is**:
  `astral_tome.cpp:475` scrims it, `chevreul_circle.cpp:409` sizes it to
  1.7× the font, `black_watch.cpp:547` to 1.6×, `cde_motif.cpp:817` takes
  a `string_view` and a colour, `ScenesNetwork.h:87` is
  `.inset(x, y, 0, 0).zIndex(8)`, `ScenesY2k.h:203` is a bare styled
  `text()` with no placement at all. Eight spellings of one word is the
  signature of a *decision*, not a spelling — compare `hex()`, which has
  three names and one body.
- **No animation grammar.** `ramp()` ships because it is six identical
  copies of a struct initialiser. A `Timeline`/`cue` DSL does not, see §5.7.
- **No `ElementNode` growth.** Every item is either a free function, a
  value type, or a method writing fields that exist.

---

## 5. What must NOT be extracted

This is the section with the most value in it. Each entry is something
that *looks* repeated and would make the studies worse.

### 5.1 The lattice / node-table resolver — the strongest rejection

`game-RETRO.md:27-33` calls this *"the most repeated construction in the
corpus and the most obviously nameable"*. I read the five cited functions
and I disagree, ruthlessly.

```cpp
// thaumonomicon.cpp:216
inline SkPoint centreOf(int col, int row) {
  return {g(kStartX + (float)col * 24 - kLocX + 8),
          g(kStartY + (float)row * 24 - kLocY + 8)};
}
// thaumonomicon.cpp:228
inline bool culled(int col, int row) {
  const float vx = (float)col * 24 - kLocX;
  const float vy = (float)row * 24 - kLocY;
  return !(vx >= -24 && vy >= -24 && vx <= kScreenX && vy <= kScreenY);
}
// astral_tome.cpp:448
inline SkPoint starAt(const Con &c, int index1) {
  const auto &s = c.stars[(size_t)(index1 - 1)];
  return {g(kUlen * (float)s.first), g(kUlen * (float)s.second)};
}
```

Three-to-five-line bodies. Now price the component that serves both:
it needs an origin, a pitch, a per-axis offset, a cull rect, a cull
*policy* (`thaumonomicon.cpp:222-227` documents that the cull is applied
to the icon's **top-left**, drops a node whose plate is more than half on
screen, and — deliberately — does **not** cull that node's edges, because
that asymmetry is what makes the browser read as a viewport onto a web
that continues), a key index, and a topology query over an edge
representation neither study shares.

That is seven parameters, one of which is a bug faithfully reproduced
from decompiled Java, to replace eleven lines. **The `+ 8` in `centreOf`
is `GuiResearchBrowser:596-600`. The `- kLocX` is a scroll offset solved
at `thaumonomicon.cpp:210-212` by hand.** A component cannot hold those
and a component that takes them as parameters has not abstracted
anything — it has moved eleven lines of arithmetic into a call site with
seven arguments.

And there are now **three** independent refutations.

**(i) The library already made this bet and it scored zero.**
`layouts::Radial` *is* "a lattice resolver as a component", and the two
studies whose entire subject is radial placement did not reach for it
(§0).

**(ii) The control group does not have this problem at all.**
`gallery/SkillTreeData.h` is the other node web in the repository — 39
nodes, 60+ edges, lifted from Path of Exile's public tree export with an
orbitRadii/skillsPerOrbit ladder that is *exactly* the lattice the retro
describes. And `SkillTreeData.h:25-32` stores `float x, y; ///< scene
space (900x640)`. The `orbitIndex → angle` resolution and the uniform
0.34928 scale were applied **when the fixture was generated and then
frozen** (`:11-15`). There is no runtime resolver because there does not
need to be one. Given a data table you control, the correct place to
resolve coordinates is in the table.

**(iii) Where a runtime web IS built, the shipped primitives already
cover it.** `ScenesSkillTree.h:174` places a node as
`box().width(d).height(d).centerAt(at)`; `:483-496` routes every edge as
`rail({{nodeKey(a)}, {nodeKey(b)}}, routers::orbit({g.x, g.y}))`.
`thaumonomicon.cpp:1397` does the same through `connector()`. The
"key index and derived topology query" is `centerAt` plus the derive
phase plus six lines of `degreeOf`.

The retro's own follow-up rule points the same way: *"a component earns a
warning when the thing it makes cheap is a choice rather than a chore."*
Placing a node lattice is a choice. Three of the first four game studies
were already node webs, and so is a gallery scene. Making node webs
cheaper is not a neutral act.

**Verdict: reject. If anything is extracted here it is `degreeOf` — six
lines of graph-degree counting — and even that depends on the study's own
edge representation.**

### 5.2 The unit map / figure-local frame

`myst-RETRO.md:18-24` proposes `Frame{centre, radius, zeroAt, sense}` with
`frame.at(θ, r)`, and claims it "would delete most of those 135
absolutes".

I checked the 135. They are not one thing:

| | `.absolute().inset(…)` | `.absolute().left(…)` |
|---|---:|---:|
| `sigillum_aemeth` | 25 | 30 |
| `thunder_fulu` | 15 | 56 |

Forty of the 135 are `.absolute().inset(0)` — full-bleed *layer groups*,
which no placement primitive addresses. The other 86 are rect placements,
which `rect()` (§4.1) addresses without knowing anything about polar
coordinates.

And the eight unit maps are 20 lines *in total* and share no signature:
`P(θ, rNorm)` is polar with 0° at 12 o'clock;
`vagrant_story_target.cpp:216-218` carries **two simultaneous snapping
grids** (4 px geometry, 2.5 px text); `eva_magi_defense.cpp:244` inverts a
CRT roll rotation; `thunder_fulu.cpp:547` is an affine map over a
*polyline*, not a point. A `Frame` serving all of them is `Layouts.h`
again with a different noun.

And the control group has **zero** unit maps in 31 files, including
`ScenesCosmati.h`, `ScenesZellige.h`, `ScenesOrnament.h` and
`ScenesFlourish.h` — the ornament family, which is the gallery's own
answer to "place by angle and radius". They do it with
`stack().width(2r).height(2r).centerAt(at)` (`ScenesCosmati.h:224-233`,
the `util::disc` shape) and 49 `std::cos`/`std::sin` calls in 10,585
lines. Whatever a `Frame` would have bought, the population that most
needed it never wrote its precursor.

**Verdict: reject the Frame. Ship `rect()`, which serves the 86 absolutes
without a coordinate-system opinion. Keep `P(θ, r)` as five lines in the
study that knows what θ means.**

### 5.3 The ring band of N cells

The most tempting thing in the corpus and the clearest reject. Verified
against `sigillum_aemeth.cpp:720-791`:

- The 40 dividers are **one node with forty contours and one trim
  window** (`.outline([]{ for i<40 … })` + `PathFormat{.trimStart = 0.09,
  .trimEnd = 0.91}`), because they are interrupted rules that stop short
  of both circles.
- The 40 letters are **forty individually keyed nodes**
  (`.key("cl" + std::to_string(i))`), each with its own radial `TextPath`
  fraction, and 7 of them carry `.opacity(with(0.30f, ramp(tDark*1000 +
  i*9, 700)))` because the *solver* decided which cells the walk never
  visited.
- The numbers ride a **different radius** depending on `c.step`
  (`rNumOut` vs `rNumIn`) — a per-cell datum recovered from the plate.

A component owning "a ring band of N cells" owns all three. The study's
entire motion is per-cell and derived from a solver the component cannot
see. `chaucer_astrolabe`'s limb (`:1580-1749`) shares the *shape* and
none of the parts: 360 instanced ticks with three length classes, 12
radial numerals, 24 hour letters each with a bound glow Output.

**Verdict: reject, and note that two researchers from unrelated domains
reached this conclusion independently — myst about the ring band, game
about `leaderTo()`. That agreement is worth more than either argument.**

### 5.4 A `ringLabel()` / `radialText()` widget

`TextPath` has **no `operator==`**, and the reconciler's rule is
conservative by construction — verified at `Reconcile.cpp:151-160`:

```cpp
  // onPath(): the baseline is an incomparable std::function, so a run
  // that has one never prunes …
  if (ta.onPath) return false;
```

So every text-on-path node re-patches on every `render()`, forever.
`sigillum_aemeth` already creates ~150 of them. A `ringLabel(str, θ, r)`
component makes it one line to create hundreds more, and the cost is
invisible at the call site. **Ship the `atDeg` fix (§4.6) — which removes
the arithmetic without making the nodes cheaper to mint — and not the
widget.**

### 5.5 The leader-line callout

Two studies built it: `astral_tome.cpp:985-1028` (dot on the star, elbow,
flat into the caption) and `vagrant_story_target` (header `:120`). Then
Vagrant Story's implementer wrote the best sentence in the corpus, at
`:135-137`:

> a column needs no leader, because a column IS the answer to "where does
> this readout go"

`astral_tome`'s leader is 44 lines and half of them compute a bounding
box so the elbow path can be authored in node-local coordinates —
plumbing that `rect()` plus a canvas-space overlay already removes.

**Verdict: do not ship `leaderTo()`. Ship the 20-line recipe in the
documentation with that sentence at the top of it.** A leader is a
component *and* a temptation; the studies that used one used it because
the geometry demanded it, not because it was available.

### 5.6 Palettes and data tables

Hex-literal counts: `xcom_battlescape` 256, `ksp_mapview` 214,
`chevreul_circle` 166, `chaucer_astrolabe` 138, `thaumonomicon` 134,
`winamp_base` 127.

Gallery hex-literal count: 178 across 31 files, plus 399 raw
`SkColor4f{…}` literals in files that never adopted a `C()` helper at all
(`ScenesAero.h:49`, `ScenesBeethoven.h:49-52`, `ScenesConsole.h:43-48`).

Every one of them was sampled off a reference frame and is the study's or
scene's primary research output. There is nothing here to extract but
`hex()` (§4.2). A shared "palette type" would add a declaration form and
remove nothing. **Reject.**

Note the asymmetry that makes this rejection safe: `hex()` is a
component *because* it has no knowledge of any palette, and a palette
type would be a component only if it did.

Likewise the shipped data tables: `sigillum_aemeth`'s
`kRing`/`kNames`/`kAngles`/`kGodNames`, `thunder_fulu`'s eleven
`int16_t` KanjiVG median arrays, `thaumonomicon`'s `kNodes`/`kEdges`,
`vagrant_story_target`'s `kLimbs`/`kDefenceRows`. These *are* the
studies.

### 5.7 A reveal-timeline DSL

168 `withFrom(0.0f, 1.0f, …)` sites across 17 files, and three studies
declare a named phase clock (`chaucer_astrolabe.cpp:744-758` 15 cues,
`thunder_fulu.cpp:601-612` 11, `chladni_tab1.cpp:389-397` 9).

I sketched the API and it saves **zero lines**:

```cpp
.opacity(withFrom(0.0f, 1.0f, ramp(tTicks * 1000 + 300 + (float)i * 25, 400)))
.opacity(reveal(cue(tTicks) + 300ms + i * 25ms, 400ms))       // proposed
```

The second is arguably nicer to read and it is not an extraction — it is
a rename with a new type to learn. **Ship `ramp()` (six identical copies,
43 lines) and stop there.**

### 5.8 The inline `outline([](SkSize){…})` generators

127 of them, 1,089 lines, 23 files — the second-largest idiom by line
count. Every one is a different silhouette: a torn square
(`thaumonomicon.cpp:667`), a MAGI plate with a stem
(`eva_magi_defense.cpp:508`), a chamfered pill with per-corner cut flags
(`:565`), a sheared CRT bar (`lain_navi.cpp:370`), forty radial dividers
(`sigillum_aemeth.cpp:728`).

`Shapes.h` already carries every silhouette that generalised —
`polygon`, `star`, `squircle`, `blob`, `sector`, `annulus`, `chamfered`,
`parametric`, `lissajous`, `rose`, `harmonograph`, `spiral`, `trochoid`.
What remains is by definition the un-generalisable residue. **The 1,089
lines are the escape hatch working.**

(One real defect adjacent to this, already in `ROADMAP.md §3`:
`outline()` can never prune, because an `OutlineFn` is an incomparable
`std::function`. That is worth fixing and is not an extraction.)

### 5.9 The 11,600 comment lines

Every study opens with a provenance block: where the reference came from,
what was measured off it, which numbers are decompiled and which are
solved. `thaumonomicon` 146 lines, `lain_navi` 270, `astral_tome` 284,
`eva_magi_defense` 175 — 3,950 lines of header before the first
`#include`, and 10,110 comment lines overall. The gallery carries 1,490
in the same voice (`SkillTreeData.h:1-16` is a model of it: the export it
came from, the two constants verbatim, the transform applied, and "this
is a study fixture, not a live import").

Together they are 19% of both populations and they are the reason either
corpus is worth anything. **Nothing in this document touches them.**

---

## 6. Constraints, checked against the source

1. **`sizeof(ElementNode) ≤ 768`** (`Composer.cpp:33`), `PaintProps`
   inline at 688 B. Every proposal in §4 either writes existing
   `LayoutProps` fields (`rect`/`at` — `ComposeInternal.h:25-41` only) or
   is a free function outside the node entirely. **No new node state, no
   `Box<>` block needed.**
2. **Live values must participate in reconciler equality.** `rect`/`at`
   write into `LayoutProps`, whose `operator==` is defaulted, so they
   prune exactly as the longhand does. `studio::type` returns a
   `weave::TextStyle`, already compared by `textEqual`
   (`Reconcile.cpp:120-171`). `console::panel` and `debug::check` compose
   existing nodes. The one place this bites is §5.4 — text on a path
   never prunes (`Reconcile.cpp:158-159`), which is why a `ringLabel()`
   widget is rejected rather than shipped.
3. **Read the source before believing it.** Three claims from the retros
   did not survive counting: the "135 hand-placed nodes" is 86 placements
   plus 40 full-bleed layer groups (§5.2); the lattice resolver is not
   the most nameable construction but the least, and the control group
   dissolves it entirely (§5.1); and `layouts::` is not unused — it has
   five uses, all in the gallery scenes written alongside it, which is a
   sharper fact than "zero" (§0). Claims that survived exactly: 64 and 71
   `.absolute()` in the two esoteric plates; `heptChords` at
   `sigillum_aemeth.cpp:422`; 53 `fmt()` calls between those two files;
   `rgb`/`mul`/`prog` in three game studies; two bitmap-font
   rasterisers.
4. **A note on the sketch corpus as evidence.** Twelve of the 33 sketches
   were written to one standing brief, so their convergences have a
   shared cause. Wherever a candidate rests only on those, this document
   says so — §1.2 is the only one that does not need to.
5. **Perf.** Nothing proposed changes a paint path. Two warnings must
   ship attached to the components that invite them:
   - **`.absolute().inset(0)` (293 sketch + 86 gallery sites, 40 files) is a full-canvas
     node.** Any binding on one is a `saveLayer` the size of the canvas —
     the measured 18.38 → 7.94 ms. If a `util::layer()` helper ships to
     name this idiom, that sentence ships in its doc comment. See also
     `eva_magi_defense.cpp:919-923`, which documents the *opposite*
     failure: `inset(0)` inside a `slot()` lays out 1920×0, "harmless for
     an outline in absolute coordinates, and a lie in every query".
   - **`alphaMask` bakes at 1× and blits with `kNearest`.** It is not
     `bakeScale` and must not be confused with it.

---

## 7. Recommendation, ordered

Ordered by evidence strength, not by line count — an item present in both
populations outranks a bigger one present in only the sketches.

| | item | evidence | saves | risk |
|---|---|---|---:|---|
| 1 | `<sigilcompose/Studio.h>` — `hex`, `Type{}`+`type`, `pickFace`, `ramp`, `fmt`, `prog`, colour ops | **both populations**: 108 definitions across 45 of 64 files, three names for one function, no shared brief between the groups | ~1,430 | none — free functions |
| 2 | `Element::rect(SkRect)` / `at(SkPoint)` + `util::centred/xywh` | 30 of 33 studies but only 6 of 31 gallery files; 9 independent reinventions; **the gallery is flex-native — ship the qualification** | ~1,630 | none — existing fields, existing equality |
| 3 | `console::panel` + `console::monoStyle` | **both**: 6 sketches + `ScenesConsole.h` | ~190 | low |
| 4 | `debug::check` + `debug::report` | 5 sketches; `minard_1869.cpp:2580` already invented it | ~130 | low; the qualitative win is larger than the line win |
| 5 | `compose::alphaMask` (and settle `Debug.h`'s name) | 2 sketches, ~180 lines with a 25-line shared core | ~50 | low; **file the `text()`-takes-no-`PropValue` gap separately** |
| 6 | `shapes::chords(n, inset)` | 1 sketch, but an exact peer of `shapes::polygon` | ~19 | low |
| 7 | `TextPath::atDeg` | 8 sketches convert an angle to a fraction by hand | ~0 lines | none; removes a convention leak, not verbosity |

**Do not build:** a frame/coordinate-system value, a lattice resolver, a
ring-band component, a `ringLabel` widget, `leaderTo`, a tick-ladder
component, a palette or theme layer, a reveal-timeline DSL, a `label()`
widget, a state-enum→visuals mapper.

**The honest summary.** Neither population is verbose because
SigilCompose is missing a framework. Both are verbose in two specific,
boring places — **how a file starts** and **how a box is placed** — and
everywhere else the length is research provenance (11,600 comment lines),
artefact arithmetic, or the escape hatch doing its job (2,025 lines of
`PaintProgram`, 1,474 of `outline()`). Items 1 and 2 are 3,060 of the
~3,750 extractable lines and neither makes a single design decision on
the author's behalf.

**What reading the control group changed.** Three things, and they all
pushed the same direction:

1. It **promoted the prelude to first place.** Twelve agents sharing a
   brief writing `hex()` is weak evidence. Twenty-four authors across two
   eras and three naming conventions writing the same four lines is not.
2. It **demoted `rect()` from universal to workflow-specific**, and
   supplied the sentence that has to ship with it.
3. It **killed the lattice resolver outright.** `SkillTreeData.h` is the
   same construction the game retro called "the most obviously nameable",
   solved by resolving the coordinates when the data was generated and
   then not needing a resolver. That is a better answer than any component
   would have been, and it came from the population that had no brief
   telling it to look for one.

The framework this corpus wants is not Next.js to its React. It is a
slightly larger `Util.h` — which is the header this codebase already
proved works, 166 times.
