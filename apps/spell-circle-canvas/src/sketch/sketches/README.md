# The sketches

One file per scene. `../README.md` is the canon for what a sketch is, how
it is registered and how it is run; this page is about what is in the
directory.

Most of these are **studies**: each rebuilds something that actually
existed — a shipped game screen, a real website, a published plate, a
paving you can walk on, a lit set — out of nothing but this repository's
own drawing libraries. They are the acceptance tests in the only form
that finds real gaps: someone trying to make a specific thing look right
and discovering they cannot.

The rule they were written under is **generated, not drawn**. Where the
original used a bitmap, the study generates the equivalent from a
material, a pattern, a distance field, a silhouette or a brush. Where the
original had a construction — a pentagrid, a conic, a cellular
automaton, a pendulum on a turntable — the study transcribes the
construction rather than tracing its output. Every file header names its
sources and splits **what is documented** from **what is
reconstruction**, because a study that blurs those is a drawing with
citations attached.

A study that hits a wall writes down **the API it wanted**, not just that
it was blocked: "no way to X, and the natural spelling is Y" is
actionable, "X was hard" is not. And check the claim before recording it
— an entry that reads "impossible" outranks one that reads "awkward", so
a wrong one distorts everything under it, and the wrong ones have all
been cases where a capable author concluded "impossible" from the
documentation without reading the source.

Run any of them:

```sh
./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
    src/sketch/sketches/<name>.cpp \
    --frame /tmp/<name>.png --at 2.5
```

`--at` picks the moment; `--frames N --fps N` writes a sequence. With no
`--frame` the app opens on it, watches the file and hot-swaps on save —
which is what you want while you are CHANGING one. Opening the app on
the whole registry is what you want while you are LOOKING at one next to
everything else.

The table below is the studies that rebuild a REFERENCE, with what each
one puts under load. Every other sketch here carries its own line in its
own `SIGIL_SKETCH` declaration, which is what the application shows
beside it — so there is one place to read and one place to change.

| Sketch | Subject | What it puts under load |
|---|---|---|
| `black_watch` | The Government sett, from Douglas's 1949 *Scotch Tartan Setts* | A tartan as CLOTH — 24 integers and a mod-4 rule, 63,504 emergent cells, ten invariants computed and printed |
| `chaucer_astrolabe` | A planispheric astrolabe of the English "Chaucer" type, computed for Oxford 51° 50′ | A working instrument that tells the time — every radius out of φ and ε, proving itself to 5.55e-16 R on the canvas |
| `cde_motif` | CDE 1.0 on OSF/Motif 2.1 (1995) | A desktop as the OUTPUT of a published function — `XmGetColors` derives four colours from one background, byte-exact including C's truncating division |
| `chevreul_circle` | Chevreul's *1er cercle chromatique*, Plate V, 1864 | The first study whose content is a PALETTE; 13 invariants computed, ten hold and three fail |
| `chladni_tab1` | Chladni's Tab. I, sound-figures of a bowed plate, engraved by Capieux 1786 | 9,580 instanced sand grains migrating onto twelve nodal geometries in one stamp, at 0.23 ms |
| `fallout2_charsheet` | The Fallout 2 character screen (Black Isle, 1998) at 2× | The program's first TYPE-SET study: ~134 positioned runs in five alignment regimes, 21/21 derived values verified |
| `ds2_bench` | *Dead Space 2*'s Bench — the Nanocircuit Repair upgrade circuit (2011) | Routers, rails and connectors; a diegetic holographic panel |
| `genesis_fire` | The Genesis Demo wall of fire (Lucasfilm, 1982) — the first particle system | Reeves' published attribute list against `instances()`; additive `kPlus` where the colour IS overlap count |
| `hitman_verlet` | Jakobsen's *Advanced Character Physics* (GDC 2001) and the Hitman ragdoll | Motion with STATE and CONTACT — and the sign error in four of the paper's five stick listings, with the reason the fifth is correct |
| `ksp_mapview` | *Kerbal Space Program*'s map view + flight instruments | Real conics with the planet at the focus; a navball as an orthographic sphere in one SkSL pass |
| `kumiko_asanoha` | A hinoki asanoha ranma — Japanese lattice joinery | 514 mitred boards; per-piece assembly staggering |
| `nightingale_coxcomb` | Nightingale's 1858 "Diagram of the Causes of Mortality in the Army in the East" | Polar-area wedges from the real mortality table; ring labels on curved baselines |
| `penrose_paving` | Penrose's 2012 decorated P3 paving, Andrew Wiles Building, Oxford | 549 setts from de Bruijn's pentagrid, zero authored geometry, self-verifying to φ |
| `slitscan_2001` | Trumbull's slit-scan machine (1966–68) and the Star Gate | A frame that is a TIME INTEGRAL — 1624 stamps summing per wall, with the 1/ρ exponent measured off an F16 read-back rather than assumed |
| `spacejam_1996` | spacejam.com, Warner Bros. Online, still live and unmodified | A DOCUMENT, not a panel — HTML auto table layout as a `LayoutScheme` matching Chrome to 0.11 px, and a 216-colour dither in `setView` |
| `psx_doom_fire` | The DOOM PlayStation title flame (1995) | A stateful cellular automaton at a fixed 27 Hz under a variable frame rate |
| `minard_1869` | Minard's own BnF presentation copy of the 1869 sheet | The plate audited against its own printed legend, then the sketch audited by the same instrument |
| `twoadvanced_equipment` | 2Advanced's Equipment.Modules store (2003), an HTML 4.0 frameset of Dreamweaver tables | The page's own bitmaps over the loader's https path, its table metrics verbatim, the styled IE scrollbar — and its only two behaviours (JS rollovers, frame scroll) as the only motion |
| `twoadvanced_v3` | 2Advanced Studios "V3 Expansions Reboot" (2024), the live Rive/React rebuild of the 2001 v3 site | The production art itself — embedded PNGs lifted from the site's own `mainstage.riv` over the loader's https path, the 62-frame cloud loop composited through a soft mask, and the section cycle replaying the stepped shape-wipe |
| `twoadvanced_v4` | 2Advanced Studios v4 "Prophecy" (2003–06) | Chamfered Flash chrome at four nesting depths, the real shell GIFs fetched from the studio's restoration host — and the MAINFRAME hero as what it was, a 3D render: a world scene of pods on water in front of a teal city, baked once and composited into the page |
| `vagrant_story_target` | *Vagrant Story*'s battle-mode targeting screen (Square, 2000) | The only study that is a SET: a lit 3D scene with a wireframe reach sphere in real space, and the whole overlay — gauges, target card, the six-limb strip — baked aliased into one texture on a quad that fills the frustum |
| `world_hud` | Veloren's HUD (github.com/veloren/veloren, voxygen/src/hud/) | Every dimension and colour read out of the source, over the thing a HUD has to stay legible on: a lit voxel valley in the same frame, with the whole HUD baked into one texture on a quad that fills the frustum |
| `xcom_battlescape` | X-COM: UFO Defense (1994), the Battlescape, at 4× | 115 colours and 115 of them in the palette; a 4× round trip with 0 mismatching pixels of 1,024,000 |
| `vertigo_titles` | Saul Bass / John Whitney's *Vertigo* titles (1958) | The precessing Lissajous derived from Whitney's M-5 gun director; hollow display type |
| `shipping_forecast` | BBC Radio 4's 0048 bulletin, whose every adjective is a defined quantity | The whole text engine as one designed sheet — a ring marquee, a nested cascade, a grade swell, a decode, mixed faces in one paragraph, and a column running down the page |
| `karaoke_wipe` | Fleischer's bouncing ball (1924) and the CD+G subcode wipe (1985) | One schedule, two conventions: an `fx::tint` cascade against a ball placed from the schedule read back with `beatsOf` |
| `axis_ripple` | The variable-font weight wave, the demo every variable face ships with | A driven `GRAD` axis, with the `wght` advance drift measured and printed as the reason the drive is refused |
| `elastic_type` | animate.css `rubberBand` and `jello` (Daniel Eden, 2013) | Two published keyframe tables run per glyph on the non-uniform scale and shear lane, with the tables plotted beside them |
| `matrix_rain` | The Matrix's digital rain (Simon Whiteley, Animal Logic, 1999) — the in-film kind, where the light falls and the type stands still | Four vertical-RL curtains of mirrored half-width katakana; per-column cue tables with nested cluster cascades, a held keyframe streak, `fx::scramble` churn through an advance-uniform charset, and per-glyph fades splitting glow underlays into fade classes — thousands of glyphs, all moving |
| `rota_convocationis` | An invented conjuring wheel in the real idiom of the Solomonic circles, Agrippa's planetary tables and the alchemical rotae | A magic circle that ASSEMBLES — fourteen curved baselines forming, orbiting and charging at once: fitted ring runs, a cue-table rim with a `beatsOf`-placed scribe, roundels chained start-to-start from `spanMs`, a kamea decoding under a nested cascade, and an `fx::pass` charge riding a marquee baseline |
| `winamp_base` | Winamp 2.91's default "Base" skin | A bitmap skin rebuilt as generated material; a genuinely quantised 28-frame slider |

Three are not studies:

| Sketch | Why it exists |
|---|---|
| `hello` | The starter. Copy it. |
| `stock_materials` | One of every stock material, painted from a sketch dylib and wired up as the `sketch_reload_materials` test — so a helper added to a shader fails the build instead of failing someone's sketch three weeks later. |
| `frame_asset` | Headless export of a frame at exact pixel size: the asset workflow's template. |
