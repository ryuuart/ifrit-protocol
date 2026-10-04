# The sketches

One file per scene — or one directory named for it, with that file as
the entry and the sources beside it built with it. [SigilSketch's
README](../../spell-circle-canvas/src/sketch/README.md) is the canon for
what a sketch is and how it is registered, and [Grimoire's
README](../README.md) for how this catalogue is built and run; this page
is about what is in the directory.

[Feature coverage](COVERAGE.md) maps public visual feature families to
examples and distinguishes sketch demonstrations from tests and host lanes.
[Presentation audit](SKETCH_REFINEMENT.md) records each catalog entry's visual
treatment and distinguishes fresh render evidence from cached previews.

The Python examples share this directory's `pyproject.toml` and `uv.lock`.
Grimoire prepares the matching `.venv` automatically, including NumPy for
the flow-field and reaction-diffusion studies. The native host supplies Sigil;
this project declares third-party dependencies. A sketch with its own nested
Python project opens independently with that project's environment.

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

Open a compiled catalog entry:

```sh
./build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
    --sketch <name>
```

Capture a native GPU plate at a named moment:

```sh
./build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
    --headless /tmp/sketch-plates --gpu --sketch <name> --at 2.5
```

The result is `plate_<name>.png` in that directory. The registry addresses
both file and directory sketches by their stem. Opening the app on a
sketch's own file — `Grimoire ../grimoire/sketches/<name>.cpp` — watches
it and swaps each save in; opening it on the whole registry lets you
inspect scenes beside one another.

Each entry's opening comment carries `TAGS:` with comma-separated subject
paths, such as `Typography/Paragraph, Motion/Transitions`. Grimoire builds
its subject tree from those paths. A sketch may have several tags; its
registration category remains its collection. File locations do not define
navigation groups.

Python sketches share the registry under the Python collection. Their
opening module docstring supplies the description, and `# TAGS:` supplies
subjects. A root `.py` file or `<name>/<name>.py` entry declaring a `@sketch`
class joins on the next build; helper modules stay out of the registry.
Its drawing code stays Python and reloads on save. A literal
`REQUIRES` tuple names optional installed modules for availability checks.

Two Python starters appear in the Python collection and under Runtime /
Starter in Subjects. `python_hello.py` draws a greeting and moving circle in
one `draw` method; edit `COLOR`, `SPEED` or the greeting.
`python_hello_compose.py` submits a retained tree once: a Python `card`
component, a gradient paint factory, a native themed page and native entrance
motion. Edit `TITLE`, `CARDS` or `card`, then save either file to reload it.
Compose examples use native fluent properties and explicit `.children(...)`
groups. `python_compose_stamps.py` places those trees with a Draw pen and
repeats a composed mark through a custom brush tip.

Feature demonstrations use the shared SketchKit feature theme, page,
section headings and comparison tracks. Compact density keeps fixed specimen
grids at their authored measure; spacious density gives explanatory pages
larger margins and headings. Bind the theme where each tree is described,
including update functions that rebuild it. Reference studies and authored
catalog reconstructions retain their source's visual language; an image whose
drawing is the subject does not need specimen furniture around it.

`document_styles.cpp` and `python_document.py` demonstrate document authoring:
headings, paragraphs, quotations and lists are native Compose Elements with
semantic roles. The same content appears under two scoped stylesheets; the C++
study also uses author classes and inline text styles for local emphasis. Change an `h1`,
`paragraph` or `caption` rule to restyle that role throughout its document.
The shared specimen page and captions use these roles too; reference studies
keep their authored faces, measures and geometry. Literal typography fixtures
still use the raw text and paragraph APIs when those controls are the subject.

`python_live_signals.py` is a live JSON signal observatory: native UDP feeds
receive normalized pressure and flow, retained cards report the latest values,
and native paths draw their history. Each valid message gets a JSON reply.
Run `python -m sigil.examples.tools.send_live_signals --export reply.json`
with the matching installed package to send signals and explicitly save the
last acknowledgment. Headless captures replay synthetic arrivals without
opening a socket or writing a file; the live waiting state labels its
reference traces until a sender arrives.

The table below is every sketch filed under a `Study ·` category — the
studies that rebuild a REFERENCE — with what each one puts under load.
Every other sketch here carries its own line in its own `SIGIL_SKETCH`
declaration, which is what the application shows beside it — so there
is one place to read and one place to change. A `Catalog ·` folder holds
reconstructions too, filed by what they are rather than by what they
were read off; the rule above is written for all of them.

| Sketch | Subject | What it puts under load |
|---|---|---|
| [painted_fields](painted_fields/README.md) | A stone and metal material workbench | Native brush/text input maps, shared height and wetness, shader slots, committed uniform arrays, erasure, clipping, reset and density placement. |
| [glass_atelier](glass_atelier/README.md) | An optical-material editor over moving content | Compose shaped backdrop glass, bounded Snell displacement, material inks, retained placement, open contours, clipping and optical identity endpoints. |
| [stone_relief](stone_relief/README.md) | Raking light on carved limestone and engraved lettering | Analytical relief normals, glyph-derived normal maps, inherited moving light, roughness endpoints, matching normal conventions and small clipped geometry. |
| [optical_liquid](optical_liquid/README.md) | Volumetric instrument menisci and floating optical layers | Live destination refraction, liquid fill endpoints, thick edges, diffuse and opaque coatings, nested clips and overlapping Compose layers. |
| [metal_instrument](metal_instrument/README.md) | Machined aluminum audio hardware and studio reflections | Real World solids, Compose face textures, normal/roughness maps, grazing and reverse views, exploded depth, near-plane clipping and a projected Compose comparison. |
| [wet_glass_console](wet_glass_console/README.md) | Water beading on an environmental instrument | Compose surface fills and ink, moving droplets over live telemetry, film, smears, refraction, opacity endpoints and clipped overlap. |
| [ceramic_glaze](ceramic_glaze/README.md) | Glazed stoneware and porcelain controls | Compose ceramic surfaces, pore and crack maps, glyph relief, roughness and normal extremes, grazing light and unlit controls. |
| [embossed_foil](embossed_foil/README.md) | Crinkled thermal blanket and embossed interface | Compose material fills and ink, combined contour and wrinkle normals, metallic reflections, signed relief, scoped lights, masks and counter-holes. |
| [struck_metal](struck_metal/README.md) | Pierced brass, enamel inlay and stamped controls | Physical relief, actual glyph counters and openings, a sliding register, ribbed controls, roughness extremes and reverse views. |
| [carved_marks](carved_marks/README.md) | Carved stone and chased copper under one letter die | A shared height field from text, ribbon and fibre marks, signed cut depth, masked ink and gilding, raking light, and height-driven material ink in Element, Glyph and Word domains. |
| [carved_marks_set](carved_marks_set/README.md) | Carved stone and chased copper painted as a surface and lit by a set | A Compose page rendered as base colour, normal, roughness, metallic, occlusion and emissive maps on one World plate; World lights and environment shade the cuts, gilt glyph ink and relief; unlit captions arrive as emission; a turning view. |
| [layered_material_type](layered_material_type/README.md) | Porcelain and precious-metal type impressed in one surface | Text blurred into a held height texture, signed bump normals, coating and overprint endpoints, counter clipping, and Glyph and Word ink under directional, Point and Spot scenes. |
| [light_table](light_table/README.md) | A material inspection desk for live light | Pointer-edited light colour, strength, bearing and elevation; Point and Spot placement, range and cones; paired scenes, a nested scene, cached and live content. |
| [luminous_layers](luminous_layers/README.md) | Emissive alloy type, pressure ribbons and painted deposits | A half-float working image with a display exposure, shoulder and clip; overbright emission, alpha calibration, layer order, glass refraction, and 8-bit against float textures. |
| [metal_linework](metal_linework/README.md) | Silver and copper routing linework and material lettering | Stroke caps and joins as coverage, pressure ribbons, brushed normals, masked plating, roughness and metallic endpoints, and a single source's colour sweep. |
| [pigment_brushes](pigment_brushes/README.md) | Layered pigment brushes, metallic lines and lettering | Variable-width ribbons and brush layers sharing materials, ridge normals, wet roughness, metallic overprint opacity and sub-pixel clipped marks. |
| [reflection_lobe](reflection_lobe/README.md) | Curved metal, letters and bristles under a shared studio | Roughness-dependent reflection spread on equal ramps, a seam-continuous panorama, per-pixel roughness maps, coating, and scoped Point lights. |
| `black_watch` | The Government sett, from Douglas's 1949 *Scotch Tartan Setts* | A tartan as CLOTH — 24 integers and a mod-4 rule, 63,504 emergent cells, ten invariants computed and printed |
| `chaucer_astrolabe` | A planispheric astrolabe of the English "Chaucer" type, computed for Oxford 51° 50′ | A working instrument that tells the time — every radius out of φ and ε, proving itself to 5.55e-16 R on the canvas |
| `cde_motif` | CDE 1.0 on OSF/Motif 2.1 (1995) | A desktop as the OUTPUT of a published function — `XmGetColors` derives four colours from one background, byte-exact including C's truncating division |
| `chevreul_circle` | Chevreul's *1er cercle chromatique*, Plate V, 1864 | The first study whose content is a PALETTE; 13 invariants computed, ten hold and three fail |
| `chladni_tab1` | Chladni's Tab. I, sound-figures of a bowed plate, engraved by Capieux 1786, printed from its copper plate | 8,480 instanced sand grains flown onto twelve nodal geometries read from a words file, stamped twice, live and baked, with a stepped value choosing which shows, so the grains hop under the bow as they gather and again on every round; a declared bow round whose sound leaves the rim as two wavefronts fading as they spread |
| `fallout2_charsheet` | The Fallout 2 character screen (Black Isle, 1998) at 2× | The program's first TYPE-SET study: ~134 positioned runs in five alignment regimes, 21/21 derived values verified |
| `ds2_bench` | *Dead Space 2*'s Bench — the Nanocircuit Repair upgrade circuit (2011) | Routers and the wires an operator builds from them; a diegetic holographic panel |
| `genesis_fire` | The Genesis Demo wall of fire (Lucasfilm, 1982) — the first particle system | Reeves' published attribute list against `instances()`; additive `kPlus` where the colour IS overlap count |
| `hitman_verlet` | Jakobsen's *Advanced Character Physics* (GDC 2001) and the Hitman ragdoll | Motion with STATE and CONTACT — and the sign error in four of the paper's five stick listings, with the reason the fifth is correct |
| `ksp_mapview` | *Kerbal Space Program*'s map view + flight instruments | Real conics with the planet at the focus; a navball as an orthographic sphere in one SkSL pass |
| `kumiko_asanoha` | A hinoki asanoha ranma — Japanese lattice joinery against a breathing andon, with its shop drawing | 514 mitred boards; per-piece assembly staggering as bindings over one clock; an exploded cell cut by the same rule |
| `nightingale_coxcomb` | Nightingale's 1858 "Diagram of the Causes of Mortality in the Army in the East" | Polar-area wedges from the real mortality table on one sheet of roles and palette tokens; ring labels on curved baselines, tint and key stones printed a hair out of register, a hanging-indent roundhand legend written line by line, and the plate bound into its book with a gutter and raking light |
| `penrose_paving` | Penrose's 2012 decorated P3 paving, Andrew Wiles Building, Oxford | 549 setts from de Bruijn's pentagrid, zero authored geometry, self-verifying to φ |
| `slitscan_2001` | Trumbull's slit-scan machine (1966–68) and the Star Gate | A frame that is a TIME INTEGRAL — 1624 stamps summing per wall, with the 1/ρ exponent measured off an F16 read-back rather than assumed |
| `spacejam_1996` | spacejam.com, Warner Bros. Online, still live and unmodified | A DOCUMENT, not a panel — HTML auto table layout as an arranging value matching Chrome to 0.11 px, and a 216-colour dither in `setView` |
| `psx_doom_fire` | The DOOM PlayStation title flame (1995) | A stateful cellular automaton at a fixed 27 Hz under a variable frame rate |
| `minard_1869` | Minard's own BnF presentation copy of the 1869 sheet | The plate audited against its own printed legend, then the sketch audited by the same instrument |
| `twoadvanced_equipment` | 2Advanced's Equipment.Modules store (2003), an HTML 4.0 frameset of Dreamweaver tables | The page's own bitmaps over SigilIO's https path, its table metrics verbatim, the styled IE scrollbar — and its only two behaviours (JS rollovers, frame scroll) as the only motion |
| `twoadvanced_v3` | 2Advanced Studios "V3 Expansions Reboot" (2024), the live Rive/React rebuild of the 2001 v3 site | The production art itself — embedded PNGs lifted from the site's own `mainstage.riv` over SigilIO's https path, the 62-frame cloud loop composited through a soft mask, and the section cycle replaying the stepped shape-wipe |
| `twoadvanced_v4` | 2Advanced Studios v4 "Prophecy" (2003–06) | Chamfered Flash chrome at four nesting depths, the real shell GIFs fetched from the studio's restoration host — and the MAINFRAME hero as what it was, a 3D render: a world scene of pods on water in front of a teal city, baked once and composited into the page |
| `vagrant_story_target` | *Vagrant Story*'s battle-mode targeting screen (Square, 2000) | The only study that is a SET: a lit 3D scene with a wireframe reach sphere in real space, and the whole overlay — gauges, target card, the six-limb strip — baked aliased into one texture on a quad that fills the frustum |
| `bg3_dice_roll` | Baldur's Gate 3's dialogue ability check, the instant after the die lands | The engine's own type surface as a picture — the SkillId ordinals in the engine's grouping, the ResolvedRollBonus row schema, and the thing that dates the UI: the modifiers are added AFTER the natural roll |
| `xcom_battlescape` | X-COM: UFO Defense (1994), the Battlescape, at 4× | 115 colours and 115 of them in the palette; a 4× round trip with 0 mismatching pixels of 1,024,000 |
| `vertigo_titles` | Saul Bass / John Whitney's *Vertigo* titles (1958) | The precessing Lissajous derived from Whitney's M-5 gun director; hollow display type |
| `shipping_forecast` | BBC Radio 4's 0048 bulletin, whose every adjective is a defined quantity | The whole text engine as one designed sheet — sea areas on a circular baseline read round by a hand that rests on the area being read, a nested cascade, a grade swell, a decode, mixed faces in one paragraph and a column running down the page, over the synopsis's pressure chart drifting and deepening in the ring, styled by one sheet of roles, classes and colour tokens |
| `karaoke_wipe` | Fleischer's bouncing ball (1924) and the CD+G subcode wipe (1985), on a lit screen in a dark room | One schedule, two conventions: a `textFx::tint` cascade over the cue table in `data/` with a second `textFx::tween` track that lifts and flares each letter as it is sung, a ball (squash and stretch, a four-beat count-in with bursting dots) and a playhead placed from the schedule read back with `beatsOf`, a ruler of `textAttach` ticks on the letters, and a phosphor glow that follows the wipe under a cached `crtOverlay` tube |
| `axis_ripple` | The variable-font weight wave, the demo every variable face ships with | A driven `GRAD` axis as a travelling crest of weight and light with a rest between passes, with the `wght` advance drift measured, printed and struck through as the reason the drive is refused |
| `elastic_type` | animate.css `rubberBand` and `jello` (Daniel Eden, 2013) | Two published keyframe tables run per glyph and on the whole word through the non-uniform scale and shear lanes, jello answering rubberBand's first squash; the words are lit latex slabs in a pool of shadow over their rest keylines, blushing in the plots' colours as the tables strain them, with the tables plotted on their keyframe offsets and a ring riding each one-body word |
| `matrix_rain` | The Matrix's digital rain (Simon Whiteley, Animal Logic, 1999) — the in-film kind, where the light falls and the type stands still | Four vertical-RL planes of mirrored half-width katakana on one sheet — a class per depth for size, haze and halation — and one clock of seconds shaped into each plane's wrapping phase; a seeded ladder over columns with nested cluster cascades and a looping keyframe streak, `textFx::scramble` churn split by a selector into two advance classes, and per-glyph fades splitting glow underlays into fade classes; the tube around it — a warm glow behind, a refresh band of added light on a wrapping bound translate, and one cached pane of glass with scanlines, sheen and vignette — and an operator's trace line typed on, held and wiped by one looping cascade in the rain's own strike-and-settle grammar, its cursor blinking on a second track; words, charsets and clocks in `data/` — thousands of glyphs, all moving |
| `rota_convocationis` | An invented conjuring wheel in the real idiom of the Solomonic circles, Agrippa's planetary tables and the alchemical rotae | A magic circle that ASSEMBLES — fourteen curved baselines forming, orbiting and charging at once: fitted ring runs, a cue-table rim with a `beatsOf`-placed scribe, roundels chained start-to-start from each track's span, a kamea decoding under a nested stagger, and an `textFx::pass` charge riding a marquee baseline |
| `winamp_base` | Winamp 2.91's default "Base" skin | A bitmap skin rebuilt as generated material; a genuinely quantised 28-frame slider |
| `astral_tome` | Astral Sorcery's constellation cluster page (Minecraft 1.12.2 mod, 2016–19) at exactly 3× | Four live star charts on one spread — the chart is square, the CELL is stretched |
| `cosmati` | The Great Pavement at Westminster — border, turned square, quincunx and one interlaced guilloche | Courses of tesserae as one piece over a lattice, a band that crosses over itself, brass letters in the Purbeck, laid in order under a moving light |
| `dunhuang_star_chart` | BL Or.8210/S.3326, the Dunhuang star chart (c. 649–684) | Reprojected from 1,460 real stars and the published projection, then checked against the published identifications — and it refuses to answer where the source does |
| `eva_magi_defense` | [*The End of Evangelion*'s MAGI defense plate](https://static.wikia.nocookie.net/evangelion/images/f/f6/Magi_%28EoE%29.png) (1997) | Six installations as one component, rotated |
| `eva_magi_deliberation` | [Evangelion's MAGI deliberation plate](https://assets.fontsinuse.com/use-media/97461/upto-700xauto/69b54994/1/jpeg/14_95tv_FUI_2.jpeg) | One routed system: a rear circular bus, three rotated instances of one square module, and an information layer over both |
| `eva_magi_interior` | Evangelion MAGI internal architecture and Ep 13 infection diagnostics | Concentric neural registers, a protected personality core and a live memory-cell infection field |
| `lain_navi` | *Serial Experiments Lain*'s Copland OS | No opaque window anywhere, and text through a fixed focal plane |
| `sigillum_aemeth` | Dee's Sigillum Dei Aemeth (1582), Sloane MS 3188 f. 30r | Solved from the angels' own jump rule — 33 of 40 cells — and the wax disc's burnish drawn |
| `thaumonomicon` | Thaumcraft 6's research browser (2018) | Edges that are stamped art, not strokes |
| `thunder_fulu` | A Thunder-Rite talisman, WRITTEN | Real stroke medians, and the foot at 7.1× the body's tempo |

These are not studies. They are named here because something outside the
directory reaches for them, so a rename has somewhere to be noticed:

| Sketch | Why it exists |
|---|---|
| `hello` | The starter. Copy it. |
| `crossing_rule` | What `sketch_reload_runs_the_file` copies with its ground colour replaced, so the entry can tell a picture drawn by the file on disk from one drawn by the host's own compiled-in copy of it. |
| `stock_materials` | One of every stock material, painted from a sketch dylib and wired up as the `sketch_reload_materials` test — so a helper added to a shader fails the build instead of failing someone's sketch three weeks later. |
| `guest_picture` | The only sketch whose subject is not in this repository: it wears whatever another application on this machine is publishing under the name `Guest`. `frames/README.md` points at it as what a subscription looks like in a scene, and its plate is the waiting card, because a capture subscribes to nothing. |
| `guest_body` | `guest_picture`'s subject on a BODY: the same publication in a surface's base-colour slot, on a screen turning in a lit set. `frames/README.md` points at it as what a subscription looks like on a body, and its plate is the waiting card it paints itself, because a capture subscribes to nothing. |
| `alpha_ground` | The one sketch grounded in `{0, 0, 0, 0}`: what `sketch_transparent_ground` sweeps, on the raster surface and the device, to prove a ground's alpha survives the clear, and what the publication door in `../README.md` points at as a frame meant to be composited. `python_alpha_ground` is its twin. |

The Evangelion studies share their type and whole-screen CRT treatment in
`eva_magi_interior/EvangelionUi.h`. The screen uses SigilMaterial's CRT kit
preset for curvature, RGB spread, bloom, raster lines and grain. Japanese display type prefers an installed Matisse EB;
otherwise it uses a heavy Japanese Mincho face. Noto Serif JP Black supplies
the fallback on the development machine. The fallback preserves Japanese
glyph forms but is not an exact substitute for Matisse.
