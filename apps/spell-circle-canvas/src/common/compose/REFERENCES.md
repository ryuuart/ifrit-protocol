# SigilCompose — reference grammar library

Distilled construction grammar from REAL sources, extracted 2026-07-21 by an
adversarial research pass (full findings with citations in the session
archive). **Every Compose example is a study of one of these grammars** —
never an invented aesthetic. When adding an example, cite its section here.

---

## 1. Persona 3 Reload — the ATLUS "sexy UI" grammar

**Palette:** abyss navy ground `#061030→#0B2A5E` (vertical); primary blue
`#1269CC`; cyan highlight `#51EEFC` (selection — cyan chosen so cyan+red
armband light = white); de-emphasis sea-blue `#6D9AC7`; paper `#FFFFFF`; ink
`#14161F–#303030`. Red is scarce (danger only). P5R swaps the axis to
`#E60012`/black/white.

**Sticker stack (the hard shadow):** zero-blur only. Bottom→top: (a) solid
ink duplicate offset +6..+10px; (b) optional cyan/white sliver offset +3
(misprint edge); (c) 4–8px white outer stroke; (d) fill. One light direction
per screen (upper-left). Selection animates offset +2px, may recolor cyan.

**Diagonal slash:** P3R skew −8°..−14° (P5R −15°..−25°, papercut edges);
lists stack along the diagonal, each row x-offset by `rowHeight·tan(skew)`;
never center — anchor corners, let the diagonal carry the eye.

**Halftone fields:** 45°-rotated dot grid, spacing 8–14px, radius modulated
0→0.45·spacing by a gradient across the field; drifts along the grid
diagonal ~8–12px/s forever. Ink-on-blue or cyan-on-navy, 15–35% alpha.

**Selection = invert + splash:** card floods `#1269CC→#51EEFC`, text snaps
white w/ cyan stroke; 8–14 teardrop droplets eject 20–60px fading 250ms; a
white crescent sweeps through a clip ~200ms plus-blend 40%; scale
1.0→1.12→1.04 overshoot ~150ms. Cursor response <100ms — snap or flow
(200–400ms ease-out-back), nothing meanders; exits ~0.7× entrance time.

**Sea-of-soul ground:** render → desaturate → posterize 5–7 levels (banding
deliberate) → duotone `#0A2038→#2FB9D8` → two summed sine UV displacements
(A≈1%, 3–5 waves/screen, 2.5–4s period; second at 1.7×f, 0.4×A) + rising
bubbles + 2–3 god-ray wedges (plus, 4–8%).

**Gauges:** sheared capsules on ink quads; fill 2-stop ramp (HP
`#51EEFC→#1269CC`); white ghost bar at 40% lags ~300ms; hard 2px ink line.

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
