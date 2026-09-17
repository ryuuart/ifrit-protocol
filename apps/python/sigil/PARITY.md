# Python authoring coverage

The package is an alpha authoring surface over the native libraries. Coverage
is explicit rather than inferred from a module name. The raw bindings and
convenience builders share native values; neither is a second scene model.

## Retained authoring

| Surface | Bound contract | Remaining boundary |
| --- | --- | --- |
| Canvas hosting | Setup, update and draw argument prefixes; checked context services; fresh local import generations; source-backed catalogue and external workspaces | Model state is replaced on reload; each workspace process uses one Python environment |
| Composition | Direct native factories and overloads, fluent properties, named inputs, variadic or iterable children (including lists and tuples), dimensions and units, layout and placement, 2D/3D transforms, fills, typography and style classes | The full native Element and feature catalogue is not claimed |
| Memo | Deep-copied model, Python equality, native reconciliation and captured inherited environment | Builder must be pure in its model and environment; closure mutation is not a dependency |
| Motion | Shared outputs, binding chains, easing values, entrances, transitions and keyframe paths | Python times use seconds; scheduling comes from the native session ticker |
| Typography | Native Type/TextStyle and paint layers/decorations; complete owned Block settings; ParagraphStyle, initial letters, TypeSheet, RichText runs/slots, Story frames, selectors/restyling and TextPath | Direct editable Paragraph/FontContext layout, custom flow/hyphenator implementations, annotations/ruby, per-glyph effect tracks and PaintLayer.material remain unbound |
| Composer | Render, named slots, bounds, hit tests, routes, settling and owned statistics | Checked session view; manual native frame driving is not exposed |
| Ticker | Regular and fixed callbacks, derivation, elapsed and activity | Checked session view; removal follows a callback's return value |
| World | Native keyed elements, mesh/material/light/camera values, motion lanes, retained Scene, CPU images and checked Pen drawing, geometry/post passes, selectors, rig/turntable/lit-set presets | Device execution, environment maps, map authoring, point operators, custom pass callbacks and readbacks remain unbound; CPU material response is approximate |
| Data and assets | Native JSON, tables, scales, database queries and checked resource loading | Database write methods are not exposed |
| IO | Native hubs, feeds, arrivals, transport registration, send/reply, byte sinks and deterministic replay | Authors poll on their update clock; transport callbacks and threads stay native |
| Specimen kit | Stage, page, well, caption, cell, aligned comparison tracks, cells, panel grid, comparable theme and native Provide | Other sketch-kit components remain separate coverage decisions |
| Compose kit | Neutral sheets, panels, boards, wells, captions, rules, circles, and native Grid plus free-form layouts | Stock routers, feeds, pools and every decoration scheme are not implied |
| Document kit | Native article/section, six heading levels, plain/rich paragraphs, lead, captions, labels, eyebrow, footer, code, quotes, lists/items, figures and rules; structural role styling and inherited layout properties | Semantic roles style font/block values through the native stylesheet; CSS parsing and arbitrary element-property selectors are not supplied |

`python_type_atelier.py` combines mixed runs, inline slots, named selections,
initial letters, balanced story frames and a curved baseline.
`python_kit_specimen.py` combines native layout schemes, page furniture and a
memo that restores its scoped theme. `python_motion_signals.py` combines
shared outputs, native bindings and keyframe motion. `python_memo_station.py`
combines retained model descriptions and native motion. These are authoring
studies rather than ports claiming pixel identity with C++ originals.
`python_compose_stamps.py` places Compose trees with a Draw pen and uses one
as a custom brush tip.
`python_document.py` places the same document under two inherited role
stylesheets. The native document kit also supplies prose and heading roles
to the Python retained studies and shared specimen furniture.
`python_data_garden.py` uses native table sorting and domain scales to draw
two views of one illustrative CSV dataset.

`SigilPython` owns the reusable native bindings under `src/common/python/`.
The `SigilSketchPython` leaf adapter adds embedding, sketch sessions and the
sketch kit. Native hosts supply `Host::Options::pythonLoader` when they opt
into `.py` entries; the core sketch library does not require Python. The
public package, declarations and Python contracts belong to `apps/python/sigil/`.
Python value ownership and callback cleanup are exercised separately from
composition's native rendering contracts.

## World boundary

`python_world_study.py` reuses one native vessel mesh under keyed scene nodes,
three dielectric base colors at fixed roughness and a moving three-point
light rig. Its
scene executes on the CPU and produces a native image. Material parameters
come from `sigil.material.kit`; meshes and cameras come from
`sigil.geometry.mesh`. No World namespace aliases those origins.

`test_world.py` checks actual rendered pixels, frame post-processing and
invalid graphs, node identity and shared mesh resources, motion ownership,
owned image/getter values and checked drawing/thread access. These contracts
do not establish device-rendering parity or port every C++ World study.

## Draw collection

This table audits the native operations used by the sketches registered in
the Draw collection and its subcollections. Ordinary model logic, arithmetic,
arrays, string rewriting and iteration are Python code. The direct bindings
remain the actual native drawing, composition, image and material types.

A mapped surface means the operations needed to translate the source are
exposed. It does not mean the original sketch has been ported or that its
Python and C++ pixels are byte-identical. The Python ports below preserve the
subjects and algorithms; floating-point evaluation and simulation arithmetic
may differ. NumPy is an optional dependency for the reaction-diffusion and
vectorized flow-field studies.

| C++ sketch | Native requirements beyond basic pen verbs | Python study |
| --- | --- | --- |
| `bristle_bloom` | Gaussian pen stream; noise; blending; Bezier curves | Not ported |
| `bristle_current` | Chance stream; blending; noise; fixed-step model update in Python | Not ported |
| `brush_botanical_study` | Brush tools, strokes, deposits and procedural helpers | Adapted: [`python_botanical_study.py`](../../spell-circle-canvas/src/sketch/sketches/python_botanical_study.py) |
| `brush_custom` | Brush tools, strokes, deposits and procedural helpers | Not ported |
| `brush_dynamics` | Brush tools, strokes, deposits and procedural helpers | Not ported |
| `brush_engine_atlas` | Brush tools, strokes, deposits and procedural helpers | Not ported |
| `brush_live_tutorial` | Brush tools, strokes, deposits and procedural helpers | Not ported |
| `brush_rain` | Brush tools, strokes, deposits and procedural helpers | Not ported |
| `brushwork_currents` | Brush tools, strokes, deposits and procedural helpers | Not ported |
| `observable_circle_packing` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_circle_packing_contained` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_fibonacci` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_fibonacci_rectangles` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_flowfield_1` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_flowfield_2` | Chance stream; batched canvas lines | Not ported |
| `observable_flowfield_3` | Chance stream; batched canvas lines | [`python_observable_flowfield.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_flowfield.py) |
| `observable_grid` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_l_system` | Chance stream; Python string expansion | Not ported |
| `observable_l_system_tree` | Python string expansion and turtle stack | [`python_observable_l_system.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_l_system.py) |
| `observable_noise` | Seeded noise and Chance stream | Not ported |
| `observable_noise_map` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_random_walker` | Chance stream; point and color arithmetic | Not ported |
| `observable_reaction_diffusion` | CPU simulation; image from RGBA buffer; scaled image drawing | [`python_observable_reaction_diffusion.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_reaction_diffusion.py) |
| `observable_reynolds_steering` | Persistent model; color modes; transforms; contour drawing | [`python_observable_reynolds.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_reynolds.py) |
| `p5_attractor_loom` | Runtime shader; field recipe; layered materials; batched paths | Not ported |
| `p5_flow_field` | Runtime shader; field recipe; shape-fitted material | Not ported |
| `p5_fractal_garden` | Runtime shader; field recipe; layered materials; batched paths and points | Not ported |
| `p5_hello` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `p5_liquid_layers` | Pattern materials; shape-fitted paint; brush stroke | Adapted: [`python_liquid_layers.py`](../../spell-circle-canvas/src/sketch/sketches/python_liquid_layers.py) |
| `p5_mixed_forms` | Radial material; inherited type; retained composition guest | Not ported |
| `p5_refractive_metaballs` | Runtime shaders; uniform arrays; blending; Bezier curves | [`python_liquid_glass.py`](../../spell-circle-canvas/src/sketch/sketches/python_liquid_glass.py) |

## Shared drawing surface

- All native p5-style constants and blend modes; color model overloads and native color values.
- Geometry verbs, variable-corner rectangles, curves, contours, clipping and dynamic dash sequences.
- Canvas path, point and mesh batches through a wrapper that expires with its pen.
- Native paths, images, material paints, shape fitting, font types and copied Skia paints.
- Offscreen graphics with explicit begin/end or a scoped draw callback; unclosed child frames close with their host frame.
- Retained composition guests identified by Python source location and optional loop index.
- Full frame pointer/key state, loop control, seeded pen noise and random streams, pure math, and copyable native Chance streams.
- Native brush tools and callbacks registered with the session callback lifetime.

Some source spellings use an adapter: stage metadata can use the native
`sigil.sketch.kit.stage` or the decorator; alpha replacement uses color components; rectangle sorting
uses Python `min` and `max`; bitmap population uses `image.from_rgba`; and a
pattern tile's texture shader becomes `Tile.paint()`. The fixed scheduler used
by `bristle_current` is exposed through `ctx.ticker.addFixed`, including the
native catch-up limit, interpolation output and status. Optional brush records
are copied when read; edits are assigned back to the tool.

## Verification boundaries

`test_pen.py` exercises real raster sessions: canvas expiry, clipping and batch
drawing, offscreen pixel retention across resize, reopening buffers under a
different host, exception cleanup, distinct retained guest call sites, color
models, dynamic dashes and seeded stream copies.
The value and brush contract suites exercise their own boundary types.
Retained composition, motion, kit and runtime suites exercise model equality,
native theme capture, scoped cleanup, shared output ownership, callback
argument prefixes and checked service lifetimes.

The four Observable ports, the liquid-glass port, and the botanical and
liquid-layer adaptations have been rendered through the standalone extension
at their declared capture times and visually inspected. The botanical
adaptation changes some vein details and the footer; the liquid-layer
adaptation uses a different canvas size.
These renders verify the Python authoring paths, not pixel identity with the
C++ originals.

The table covers Draw collection authoring. It does not assert that every
unrelated sketch importing a pen also has all of its data, networking, UI,
typography, video or world dependencies bound. Those libraries retain their
own coverage boundaries.
