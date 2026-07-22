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

| Sketch | Subject | What it puts under load |
|---|---|---|
| `black_watch` | The Government sett, from Douglas's 1949 *Scotch Tartan Setts* | A tartan as CLOTH — 24 integers and a mod-4 rule, 63,504 emergent cells, ten invariants computed and printed |
| `chladni_tab1` | Chladni's Tab. I, sound-figures of a bowed plate, engraved by Capieux 1786 | 9,580 instanced sand grains migrating onto twelve nodal geometries in one stamp, at 0.23 ms |
| `ds2_bench` | *Dead Space 2*'s Bench — the Nanocircuit Repair upgrade circuit (2011) | Routers, rails and connectors; a diegetic holographic panel |
| `genesis_fire` | The Genesis Demo wall of fire (Lucasfilm, 1982) — the first particle system | Reeves' published attribute list against `instances()`; additive `kPlus` where the colour IS overlap count |
| `ksp_mapview` | *Kerbal Space Program*'s map view + flight instruments | Real conics with the planet at the focus; a navball as an orthographic sphere in one SkSL pass |
| `kumiko_asanoha` | A hinoki asanoha ranma — Japanese lattice joinery | 514 mitred boards; per-piece assembly staggering |
| `nightingale_coxcomb` | Nightingale's 1858 "Diagram of the Causes of Mortality in the Army in the East" | Polar-area wedges from the real mortality table; ring labels on curved baselines |
| `penrose_paving` | Penrose's 2012 decorated P3 paving, Andrew Wiles Building, Oxford | 549 setts from de Bruijn's pentagrid, zero authored geometry, self-verifying to φ |
| `psx_doom_fire` | The DOOM PlayStation title flame (1995) | A stateful cellular automaton at a fixed 27 Hz under a variable frame rate |
| `twoadvanced_v4` | 2Advanced Studios v4 "Prophecy" (2003–06) | Chamfered Flash chrome at four nesting depths; the whole skeuomorphic vocabulary |
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
