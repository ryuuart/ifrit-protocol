# SigilCompose — reference grammar library

Distilled construction grammar from REAL sources, extracted 2026-07-21 by an
adversarial research pass (full findings with citations in the session
archive). **Every Compose example is a study of one of these grammars** —
never an invented aesthetic. When adding an example, cite its section here.

---

## 1. Persona 3 Reload — the ATLUS "sexy UI" grammar

**Recreation-verified (2026-07-21):** cross-checked against four
independent P3R pause-menu recreations with source (Ultipuk Godot ★23 —
frame-by-frame study; deltea Svelte — ships the REAL fonts; LuisArteaga02
CSS; emanoelqueiroz CSS) plus P5 recreations for the shared grammar.
File-level citations in the session archive. P3R-specific vs P5-inherited
is now split — several earlier claims were P5 bleeding into P3R.

**Palette (pause):** ground `#015FCC` blue over dark `#031F64`; unselected
menu text cycles THREE cyans `#16CFFB / #7DE6FD / #77FEFC`; paper white;
ink black. Selection accents are PINK `#FD77D9` + RED `#F00` (see below).
P5R swaps the axis to `#E60012`/black/white. Field-HUD cyan `#51EEFC` +
blue `#1269CC` stay for gauges.

**Sticker SCATTER (corrects the uniform-skew claim):** each menu row gets
its OWN rotation, magnitude decaying down the stack, LAST item flipped
positive — three independent traces agree: deltea −25,−15,−20,−15,0,−14,
−8,−5,+8; Ultipuk −24.6,−9.2,−17.9,−17.4,+1.8,−12.6,−7.5,−1.5,+12.5;
Luis −28,−14,−20,−18,0,−12,−6,−4,+8. Plus per-item x-jitter (0..−80px),
y-jitter (0..+55px), overlapping stacks (rows overlap ~40% of height),
shuffled z. The grammar is "scattered stickers converging to straight",
not one skew. The clean ~11° diagonal DOES exist — as the white side
panel's edge (x = .210 + .194·y). Menu type: 84px, ×0.82 condensed,
italic, tracking ≈ −0.14em (real fonts: FOT-Rodin Pro EB/DB/UB, NewRodin
Pro DB, Skip Std B; free stand-ins: Montserrat XBold Italic).

**Shadows (corrects the sticker-stack claim for P3R pause):** chrome text
carries SOFT under-glow (0 ±1px 6px black 50%, four directions + 2px
`#5D6A88` outline ring); the ONLY hard offset is the red misprint echo
(`3px −6px 0 #F00`). Zero-blur multi-offset sticker stacks are P5
grammar (P5 recs: 4px 4px 0 / 6px 6px 0).

**Selection (pause menu — corrects "cyan flood"):** text snaps BLACK; a
white sliver-triangle WEDGE slides behind (width ≈ 0.5·chars+1.5 em,
rotate ≈ +8°) over a pink `#FD77D9` back-wedge; a RED text echo clipped
to the wedge (mask). Scale 1→1.5 in 100ms spring/back-out; deselect
instant; cursor move 60ms. Idle heartbeat: wedge 1→1.05 (100ms) →1
(50ms) every 600ms. Two-triangle cursor: red under white, offset ~(1,5)px
+2° (misprint), root −16°, additive. (The cyan card-flood belongs to
LIST SUBMENUS at best — no recreation implements it; treat as
unverified.)

**Entrance choreography:** items fade + DROP from −30px over 0.4s,
staggered 33ms per item BOTTOM-UP; cursor spawns at +0.4s with a damped
diagonal overshoot along (1,−1): +40 → −20 (0.2s) → +10 (0.1s) → 0
(0.1s). Menu-open "swoosh" = hand-drawn 10-frame flipbook at 50ms/frame,
frames 1–5 masking the FROZEN gameplay frame into the sweep.

**Sea-of-souls backdrop (exact layer order, Ultipuk):** frozen frame →
5-stop luminance LUT (0/.309/.480/.768/.813 = `#060A2B #0C124C #1B38F0
#3351FF #FCFEFE`) → sine UV distortion (amp 2%, ~2 waves/screen, ~12.6s
period) → flat tint mix toward `#007FD2` at 82% → two caustic layers
(same noise texture offset .5 UV, alpha = step(cut, |p1−p2|-diff): light
cut .48 α.47 `#96E3F0`; bubbles cut .79 α.255 `#54EDEA`, rising .07–.13
UV/s) → gaussian σ1.4 at ⅓ res → dark `#00106D` gradient bottom + cyan
`#00FCF2` α.39 gradient top. CRITICAL TEXTURE: caustics step time at
6 Hz (floor(t·6)/6) — the water is QUANTIZED, "we imagine the
interpolation ourselves". Unselected text ripples (amp .004, sin(y·100 +
t)) + an additive flare scrolling .05 UV/s.

**Submenu transitions:** the blot wipe — a wavy circle (edge = .1·sin(5·
(angle − progress·.4π)), 5 waves rotating as it grows), outer 0.3s, inner
copy delayed .15s over .2s showing the scene at ×0.6 UV tinted (r×.25,
g×.5, b). Back = double-circle mask, inner radius 0.8×.

**Chrome:** giant rotated index numeral of the selection (≈37vh, rotate
90°, gray `#787878–#808080`, tracking −0.2em); right-anchored tooltip
title over a "Command ────" rule tracking the title width; button prompts
= 32px circles `#031F64`/80% w/ 3px white + 3px `#5D6A88` rings; money =
white card, 2px black border. Clock/date/moon-phase are FIELD-HUD, not
pause chrome.

**P5-inherited (shared) vs P3R:** shared — ~100ms scale-overshoot
selection, idle pulse, per-item tilt, wedges, staggered overshoot
entrances. P5-only — `#E60012` axis, zero-blur sticker stacks, wiggling
clip-path selection blobs (3-pose polygon morph 0.4s), skewed flood bars
(skewX −16°, scaleX 0→1 in 0.18s cubic-bezier(.7,0,.3,1)), halftone
drift (bg-position ~6.6px/s diagonal — NO P3R recreation builds halftone;
our halftoneRamp backdrop is a P5 borrowing, keep it deliberate).

**Gauges (field HUD, unchanged):** sheared capsules on ink quads; fill
2-stop ramp (HP `#51EEFC→#1269CC`); white ghost bar at 40% lags ~300ms;
hard 2px ink line.

## 2. Y2K web / graphic design (1998–2005)

**Gloss physics (the era's one rule):** vertical light model — body ramp
with a HARD stop at 45–55% height (the gloss break); a white top-highlight
lens (α .65→.85→0, top 40–50%, inset 4–8%, radius ≈ shape r − inset); light
emerging from BELOW (bottom glow — the Aqua signature).

**Aqua pill (Apple 2000):** true pill r=H/2. Body linear 90°
`rgba(28,91,155,.82) → rgba(108,191,255,.9)@0.9 → #7ECBFF`; bottom glow
inset 2px, `#B0E5FF` from bottom fading by 45%, blur σ2, screen; lens rect
x∈[5%,95%] y∈[4%,52%] white .72→0; rim 1.5px per-side
`#8BA2C1/#5890BF/#4F93CA/#768FA5`; halo `rgba(66,140,240,.5)` (0,10) blur 16.

**Sunset chrome ramp (type/logos):** `0 #EAF6FF, .12 #9CCFF3, .35 #3C7FC0,
.495 #0B2A52 ‖HARD‖ .505 #7A4A1A, .62 #B98A46, .82 #E8CE9A, 1 #FDF6E3` +
white specular sliver straddling the horizon. **Silver chrome:** `0 #FDFDFD,
.2 #D2D8DD, .48 #A5ADB5, .5 #6F7880 ‖ .52 #E9ECEF, .8 #C6CDD3, 1 #9BA3AC`,
1px white top edge, 1px `#5A6068` bottom.

**Brushed metal:** base `#B0B0B0` + mono noise ±9% + horizontal motion blur
(σ≈(12,0.5)) + vertical `#C6C6C6→#9C9C9C` overlay 15–25%; edges 1px
`#E6E6E6`/`#6E6E6E`; rivets = 6px radial `#F0F0F0→#8A8A8A` + 1px rim.

**Plastic bevel (GIF-button era):** flat web-safe fill; 2px outset — top+left
= fill lerped 45%→white, bottom+right = fill×0.55, optional 1px black
keyline; `:active` swaps edges + offsets label (1,1).

## 3. Photoshop layer styles — computation + presets

**Stack order (fixed):** dropShadow < outerGlow < ‹fill @fillOpacity› <
patternOv < gradientOv < colorOv < satin < innerGlow < innerShadow < stroke
< bevel shadow plane < bevel highlight plane.

**Bevel & Emboss:** heightmap from alpha (Smooth = dilate+blur; ChiselHard =
distance transform), lit by distant light (angle+altitude); highlight plane
Screen white ~50–100%, shadow plane Multiply black ~50–65%; gloss contour =
LUT on shading (Ring = chrome liquid rim). Compose approximation: two
opposed inner-shadow planes (ours: `styles::BevelEmboss`).

**Satin:** blur alpha, two opposite offsets, |difference|, invert, clip
inside, Multiply — the liquid sheen.

**Preset "Y2K Chrome":** fill `#C0C8D0`; GradientOverlay linear 90°
`(0,#F4F7FA)(0.35,#97A1AC)(0.49,#3A4654)‖(0.51,#1E2833)(0.62,#5C6B7C)
(1,#DCE4EA)`; bevel ChiselHard size 18 depth 220% angle 120 alt 65, gloss
RingDouble, hi Screen #FFF, sh Multiply 65%; stroke outside 2px `#10141A`;
inner shadow `#001020` 30%; drop `#000` 45% d6 s10.

**Preset "Aqua Gel":** fill `#1E8FFF` pill; InnerShadow `#003A80` 45% 90°
d=.08H s=.25H; InnerGlow Screen `#BFE4FF` center 35% s=.3H; Satin Screen
white 25% angle 19; GradientOverlay Screen 55% white .85→.1→0 by 50%;
Stroke inside 1px `#0B5CC4` 60%; Drop `#083060` d=.06H s=.2H.

## 4. Islamic geometric pattern (girih) — Hankin's polygons-in-contact

**The rule:** tile the plane with polygons; at every edge MIDPOINT grow 2
rays into each adjacent tile at contact angle θ from the edge; rays meet →
the strapwork. The tiling is scaffolding and is erased. Crossing angle =
180−2θ (θ=54 → the 72°/108° Lu–Steinhardt measured on real girih tiles).

**θ families:** 10-fold {36 obtuse, **54 median** (Topkapi), 72 acute};
8-fold {22.5, **45**, 67.5}; 12-fold on 4.6.12 {45, 60, 75}. Star on a
regular n-gon: {n/k} with k = θ/(180/n), tips at edge midpoints — octagon
θ=45 → {8/2} khatam (two squares = Breath of the Compassionate); decagon
θ=54 → {10/3} Darb-i Imam star.

**Lattices (edge a):** 8-fold = 4.8.8, octagons on square lattice
s = a(1+√2), squares (rot 45°) at cell centers — octagon apothem = s/2
exactly. 12-fold = 3.12.12, s = a(2+√3), triangular lattice. 10-fold =
decagons (inradius 1.539a) on rhombic lattice 3.078a + one bowtie filler
per cell (cell minus decagons). The 5 girih tiles (decagon, pentagon,
elongated hex, bowtie, rhombus — all edges a, angles multiples of 36°)
decorate THEMSELVES under PIC θ=54.

**Inference (Kaplan):** per tile, enumerate ray pairs; crossing candidates
cost |OiP|+|POj|, collinear-facing cost |OiOj|; sort, accept greedily while
both rays unused. Second pass inside a large n-gon's empty core → rosettes.

**Zellige palettes:** Fez — star `#1B4B9B`, cross/ground `#14766B`, darts
`#9E3B2B`, gold `#D6A419`, straps bone `#E9E0CB` outlined `#2E211B`.
Nasrid — core `#A96F2E`, petals `#EAE3D0`, kites `#34549C`, greens
`#4E7E4E`/`#26201C`.

## 5. Skill-tree / node-graph linework (brushes between elements)

**The 3-state law (PoE, verified in Path of Building source):** every edge
∈ {Normal, Intermediate (one end / hover-path), Active}; assets literally
`LineConnector{Normal|Intermediate|Active}` + `Orbit{n}{State}` arcs; orbit
radii {0,82,162,335,493}px, skillsPerOrbit {1,6,12,12,40}. Hover promotes
the whole would-be path one state; would-be orphans tint multiply red.

**PoE rope palette:** Normal body `#3A332A`/ridge `#57493A`; Intermediate
`#6B5A40`/`#8D7550`; Active `#8A7248`/`#C7A86B` + spec `#EBD6A4` + halo
`#FFC970` α.13 σ6. Rope recipe: body w11; two counter-dashed strand layers
w7 dash{7,5} phases 0/6; ridge light w2 dash phase 3.

**Ori filament (the organic glow signature):** 4 strokes bottom-up —
w14 σ8 `#6FD8FF` 18% ⊕, w7 σ3 `#9FE8FF` 45% ⊕, w2.5 `#DFF6FF` 90%,
w1 white 70%. Glow envelope 4–6× core width. Locked branch = whole stack
at 25% desaturated toward `#7C86A8`. Unlock = partial-path sweep 400ms.

**FFX sphere grid:** lattice 3px steel `#8FA6C6` 60% + 1px groove; traveled
band UNDER nodes OVER lattice, 7px round caps, per-character color @85%
(Tidus `#35C8D8`, Yuna `#E8ECF2`, Auron `#D8353F`, Wakka `#E8C838`, Lulu
`#B048C8`, Kimahri `#3858B8`, Rikku `#48C858`); movement extrudes the band
end-length 250ms; activation = white flash 1.0→1.6× fading 300ms.

**P5R ink stroke:** black/white/red only, NO blur/gradient; jagged
polyline (subdivide 14–24px, jitter ±3–6px), variable width 2–7px within
one stroke, white copy offset (2,2) behind; 2-frame boil at 6–8Hz.

**FUI circuit:** orthogonal + 45° chamfers (8px), tiers 1/2/4px
(data 55% / main 85% / power 100% + 8px underglow); via = ring r5 stroke
1.5 + bg hole r2; chevron flow: 6×8px stamp every 28px marching 120–180px/s.

**Width law:** state changes shift color/brightness dramatically but width
by ≤1.3× — hierarchy encodes importance, state encodes progress.

**Pulse-travel:** 40–60px bright window at 240–400px/s, 1.5–3s gaps,
random phase per edge (async shimmer); profile transparent→color→white
core→transparent.

## 6. Windows Vista / 7 Aero glass (DWM, reverse-engineered)

**The colorization formula (verbatim from the recovered Win7 pixel shader):**
`out.rgb = tint·colorBalance + tint·luma(blur)·afterglowBalance + blur·blurBalance`
(luma = Rec.601). Three additive layers: flat tint, "afterglow" (tint
modulated by the blurred backdrop's luminance — why bright windows behind
glow through in the accent), and the raw blur. **Win7 "Sky" defaults
(registry):** tint `#74B8FC` α≈42%, balances 8/43/49. Vista default:
`#409EFE` α≈27%, balances 5/27/68 (far more blur-dominant). All 16 Win7 +
8 Vista accent presets captured in the session archive.

**Blur is TIGHT, not acrylic-huge:** σ = 3.0 px (registry blurdeviation 30).
**Frame anatomy:** 1px black α.65 silhouette; 1px white α.5–.6 glass edge
inside it; around the client hole: 1px black α.35 then 1px white α.45.
Top corners r≈6px. **Sheen:** one desktop-space diagonal reflection texture
(25–35°, peak α≈.2) sliding at 13% parallax as windows move + radial white
corner glows (α.35→0 over 30px). **Caption text:** black over a white haze
halo (blur σ≈5, α.9 — DrawThemeTextEx iGlowSize≈15). **Close-button hover
bloom:** radial `rgba(255,196,180,.95)→rgba(230,110,90,.9)@35%→
rgba(190,25,20,.85)@70%→0`, ~100ms in / 250ms out; min/max in blue.
**Start orb:** 3-frame sphere — radial base `#163A5F→#0B2340@70%→#04101E`,
rim strokes, top lens, hover bloom.

## 7. Josef Müller-Brockmann (measured from archival scans)

**Beethoven 1955 — the arc system (normalized to width W, H=1.4144W;
center C = (0.2693W, 0.7156H); 11.25° module = 32 steps/turn; widths
double 1:2:4:8:16 with u = 0.0140W, hairline gap 0.0033W):**

| ring | r_inner | r_outer | width | visible runs (start→end, math convention) |
|---|---|---|---|---|
| R1 | .3480W | .3607W | 1u | −33.75→+22.5; −146.25→−135 |
| R2 | .3640W | .3913W | 2u | −22.5→+45; −157.5→−90 |
| R3 | .3940W | .4473W | 4u | 0→+78.75; −135→−45 |
| R4 | .4500W | .5560W | 8u | −150→+90 |
| R5 | .5600W | .7770W | 16u | −150→+45 |
| R6 | .7820W | 1.2070W | 32u | −150→+101.25 |

Every end angle is an exact multiple of 11.25° (measured deviation <0.3°).
Ink on paper `#F5F3EE`-ish. Skia mapping: drawArc(start=−a_end, sweep=
a_end−a_start) — or stroked-circle outlines + trim().

**The modular-grid method (Grid Systems):** baseline pitch L from
body/leading (10/12pt → L=12); type area in whole lines; field depth = n
lines with the ROW GUTTER exactly 1 empty line (59 lines = 4×14 + 3×1);
column gutter = 1 em of body; margins unequal, bottom largest; ALL element
heights snap to multiples of L; ≤3 type sizes, prefer 2 with a 3:1 jump;
7–10 words/line. Musica Viva constructions: φ²-ratio circle chains,
kMultiply overprints (`#799390`×`#A67B5B` on `#DEDCC3` → `#5F4534`),
45°-rotated lattices (period .171W, bars .116W + accents .050W).
**Motion inference (derived):** animate in the poster's own units —
sweeps quantized to 11.25° modules, ring reveal durations doubling
(120/240/480/960ms), offsets snapped to L; steps/linear, never decorative
eases.

## 8. Contemporary kinetic typography (the community grammar)

**THE reveal curve:** ease-out-expo `cubic-bezier(0.16, 1, 0.3, 1)`
(= 1−2^(−10t)). Overshoot: back.out(1.7) = `cubic-bezier(0.34,1.56,0.64,1)`,
f(t)=1+c3(t−1)³+c1(t−1)² with c1=1.70158; punchier c1 2–3. Elastic:
2^(−10t)·sin((10t−.75)·2π/3)+1.

**The moves (with circulating parameters):** MaskedRise — from dy=+1.05·
line-height under a line-box clip, rotate 6° at left-baseline; lines dur
0.9s each 0.12s, chars 0.5s/0.03s; exit up at 0.6× dur. CharPop — scale
0→1 back.out(1.7), dy +0.3em, alpha over first 35% ONLY (never overshoot
alpha). WaveFloat — dy = 0.1em·sin(2πt/1.6s − 0.5i), amplitude ≤0.15em.
TrackingIn — dx = (i−(n−1)/2)·Δ(t), Δ from −0.5em→0 expo.out 1.6s, whole
word as one organism (no stagger). WeightPump — wght 500±200·sin, advances
FROZEN at median weight (bucket ~24 weights, cache per bucket). Ticker —
duplicate strip 2×, dx = −(v·t mod stripWidth), 50–120px/s, LINEAR always;
hover → timeScale 0.25 over 0.4s, never hard-stop. BlurUp — σ 4→0 +
alpha + dy 20→0, quantize σ to 0.25px steps on cached layers.

**The stagger law (GSAP semantics — our Stagger mirrors it):** each XOR
amount (amount-mode clamps paragraph totals: entrance budget ≤1.2s);
from start|center|end|edges|random; distribution ease applies to START
TIMES, not motion. 30–50ms/char reads fluid; 100ms+ reads deliberate.
**Composition law:** alpha finishes in the first 30–40% of local progress
(opaque while still moving); overshoot only on geometry; loops run at
60–80% of entrance amplitude.

## 9. Line & brush grammar (verified against shipped implementations)

All numbers checked against the actual sources — leaflet-polylineoffset /
-polylinedecorator, mapbox-gl line shaders + v8 spec, Chromium
decoration_line_painter.cc, Firefox nsCSSRendering.cpp, openstreetmap-carto
roads.mss, rough.js renderer.ts, tldraw arrowheads.ts, QGIS
qgslinesymbollayer/qgsinternalgeometryengine.

**Arrowheads (the one true convention):** tip sits AT the path endpoint,
head extends BACKWARD over the run — decorator builds [barb, tip, barb]
on the sample; tldraw nudges the base back `clamp(len/5, 1w, 3w)`; D3
sets refX to the tip. Nobody ships SVG's raw tip-forward refX=0 default.
Apex 60° (barbs at ±tan30°·len). Body is CLIPPED/TRIMMED under every
closed head (tldraw skips only the open stroke 'arrow') — dashes must
stop under the head. Bar = perpendicular stroke centered on the endpoint;
Dot centered (tldraw insets 45%). Connectors back off bound targets
(~10px) so heads never enter the node.

**Parallel/cased lines:** two constructions in the wild — offset GEOMETRY
(leaflet: per-segment translate + join repair; circular arcs on outer
joins, intersection-trim on inner) and one-band-two-rails (mapbox
line-gap-width: extrusion inset = gap/2, outset = gap/2 + width — the
GAP is the inner CLEAR width, not center-to-center). Round joins
everywhere (osm-carto). Skia's stroke-outline gives exact parallels; run
PathOps Simplify() on the loop for tight-bend self-intersections, and
build DASHED rails per-offset so both rails share one parameterization
(dash phase stays aligned).

**Squiggle ratios:** consensus half-amplitude:wavelength = 1:6 … 1:4
(Chromium wavy ≈ λ/5 via one cubic per λ, spelling squiggle ≈ λ/4 at
λ=6px; Firefox 45° zigzag ≈ λ/6). Wavelength is a MAXIMUM: snap so an
exact wave count fits the run and endpoints land ON the route (QGIS
triangular/square/roundWaves rule). rough.js: maxRandomnessOffset 2,
roughness 1, two passes (full + half offset), offset clamped to len/10,
gain fades 1→0.4 over len 200→500.

**Railways:** the carto railway is dark line + white dash overlay, NOT
ties — osm-carto: #707070 at 2/3/4px (z12/13/18) under white 0.75/1/2px
dashed 8,8 (≈1/3 width, 50% duty). Tie-style hashed lines: QGIS default
interval:length = 1:1; osm miniature rail: 1px dash / 11px period, tie
reach ≈2.5× the centerline width.

**Proven patterns still queued:** interval-repeated direction chevrons
(decorator's {offset, repeat in px-or-%}); the QGIS placement grammar
(Interval | Vertex | First/LastVertex | InnerVertices | CentralPoint |
SegmentCenter); one-sided offset lines (mapbox line-offset: + = right of
travel — landed as lines::offsetAlong); line-pattern (sprite tiled along
the run); line gradients across the width (QGIS lineburst — fill the
stroke loop with a shader) and along the arc (mapbox line-gradient).
