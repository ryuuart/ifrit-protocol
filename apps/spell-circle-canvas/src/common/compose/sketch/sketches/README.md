# The study sketches

Each of these rebuilds something that actually existed — a shipped game
screen, a real website, a published plate, a paving you can walk on — with
nothing but SigilCompose. They are the library's acceptance tests in the
only form that finds real gaps: someone trying to make a specific thing
look right and discovering they cannot.

The rule they were written under: **generated, not drawn**. Where the
original used a bitmap, the study generates the equivalent with
`Material::`, `patterns::`, `sdf::`, `shapes::`, `brushes::`. Where the
original had a construction — a pentagrid, a conic, a cellular automaton,
a pendulum on a turntable — the study transcribes the construction rather
than tracing its output. Every file header names its sources and splits
**what is documented** from **what is reconstruction**, because a study
that blurs those is a drawing with citations attached.

Run any of them:

```sh
./build/bin/Release/ComposeSketch \
    src/common/compose/sketch/sketches/<name>.cpp \
    --frame /tmp/<name>.png --at 2.5
```

`--at` picks the moment; `--frames N --fps N` writes a sequence. Windowed
(no `--frame`) watches the file and hot-swaps on save.

**Every study is also a ComposeGallery scene.** The gallery compiles these
files in (`SIGIL_SKETCH_STATIC` — see `../README.md`), so they appear in
its sidebar under `Study · …` folders alongside the catalog scenes, with
live frame metrics and the clock controls. Nothing is duplicated: this is
the same file, and a study fixed here is fixed there.

```sh
ComposeGallery.app/Contents/MacOS/ComposeGallery                # browse
ComposeGallery.app/Contents/MacOS/ComposeGallery \
    --headless out --scene penrose_paving                       # one PNG + timings
```

Use ComposeSketch when you are CHANGING a study (hot reload, its stderr
audit), the gallery when you are LOOKING at one next to everything else.

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
| `twoadvanced_v4` | 2Advanced Studios v4 "Prophecy" (2003–06) | Chamfered Flash chrome at four nesting depths; the whole skeuomorphic vocabulary |
| `xcom_battlescape` | X-COM: UFO Defense (1994), the Battlescape, at 4× | 115 colours and 115 of them in the palette; a 4× round trip with 0 mismatching pixels of 1,024,000 |
| `vertigo_titles` | Saul Bass / John Whitney's *Vertigo* titles (1958) | The precessing Lissajous derived from Whitney's M-5 gun director; hollow display type |
| `winamp_base` | Winamp 2.91's default "Base" skin | A bitmap skin rebuilt as generated material; a genuinely quantised 28-frame slider |

Two are not studies:

| Sketch | Why it exists |
|---|---|
| `hello` | The starter. Copy it. |
| `stock_materials` | The **split-Skia guard**. It paints one of every stock SkSL material from a sketch dylib and is wired up as the `compose_sketch_stock` ctest, so a helper function added to a shader in `Patterns.h`, `Material.h` or `Sdf.h` segfaults the build instead of segfaulting someone's sketch three weeks later. |
| `frame_asset` | Headless export of a frame at exact pixel size. |

## What came out of them

`../../ROADMAP.md` is the list, ordered by how many independent studies
asked for the same thing. Roughly half the entries are now closed, and the
two most useful things learned about writing that list are worth repeating
here:

- **A study that hits a wall must write down the API it wanted**, not just
  that it was blocked. "No way to X, and the natural spelling is Y" is
  actionable; "X was hard" is not.
- **Check the claim before ranking it.** FOUR entries turned out to be
  wrong — `PathFormat` has always carried its own trim window, there has
  always been a bound `Fill`, the whole brush vocabulary has always worked
  on hand-built geometry via `decorations::paintOn`, and `onPath` walked
  every contour hours before a doc comment said otherwise. In each case a
  capable author concluded "impossible", worked around it, and the
  workaround got recorded as a gap. An entry that reads "impossible"
  outranks one that reads "awkward", so a wrong entry distorts everything
  below it. Three of the four were caught only once researchers started
  reading the library's SOURCE rather than its documentation.
