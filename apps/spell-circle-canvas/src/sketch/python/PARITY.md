# SigilDraw authoring coverage

This table audits the native operations used by the sketches registered in
the Draw collection and its subcollections. Ordinary model logic, arithmetic,
arrays, string rewriting and iteration are Python code. The direct bindings
remain the actual native drawing, composition, image and material types.

A mapped surface means the operations needed to translate the source are
exposed. It does not mean the original sketch has been ported or that its
Python and C++ pixels are byte-identical. The Python ports below preserve the
subjects and algorithms; floating-point evaluation and simulation arithmetic
may differ. NumPy is an optional dependency for the reaction-diffusion study.

| C++ sketch | Native requirements beyond basic pen verbs | Python study |
| --- | --- | --- |
| `bristle_bloom` | Gaussian pen stream; noise; blending; Bezier curves | Not ported |
| `bristle_current` | Chance stream; blending; noise; fixed-step model update in Python | Not ported |
| `brush_botanical_study` | Brush tools, strokes, deposits and procedural helpers | Adapted: [`python_botanical_study.py`](../sketches/python_botanical_study.py) |
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
| `observable_flowfield_3` | Chance stream; batched canvas lines | [`python_observable_flowfield.py`](../sketches/python_observable_flowfield.py) |
| `observable_grid` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_l_system` | Chance stream; Python string expansion | Not ported |
| `observable_l_system_tree` | Python string expansion and turtle stack | [`python_observable_l_system.py`](../sketches/python_observable_l_system.py) |
| `observable_noise` | Seeded noise and Chance stream | Not ported |
| `observable_noise_map` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `observable_random_walker` | Chance stream; point and color arithmetic | Not ported |
| `observable_reaction_diffusion` | CPU simulation; image from RGBA buffer; scaled image drawing | [`python_observable_reaction_diffusion.py`](../sketches/python_observable_reaction_diffusion.py) |
| `observable_reynolds_steering` | Persistent model; color modes; transforms; contour drawing | [`python_observable_reynolds.py`](../sketches/python_observable_reynolds.py) |
| `p5_attractor_loom` | Runtime shader; field recipe; layered materials; batched paths | Not ported |
| `p5_flow_field` | Runtime shader; field recipe; shape-fitted material | Not ported |
| `p5_fractal_garden` | Runtime shader; field recipe; layered materials; batched paths and points | Not ported |
| `p5_hello` | Core pen geometry, colors, seeded streams and transforms | Not ported |
| `p5_liquid_layers` | Pattern materials; shape-fitted paint; brush stroke | Adapted: [`python_liquid_layers.py`](../sketches/python_liquid_layers.py) |
| `p5_mixed_forms` | Radial material; inherited type; retained composition guest | Not ported |
| `p5_refractive_metaballs` | Runtime shaders; uniform arrays; blending; Bezier curves | [`python_liquid_glass.py`](../sketches/python_liquid_glass.py) |

## Shared drawing surface

- All native p5-style constants and blend modes; color model overloads and native color values.
- Geometry verbs, variable-corner rectangles, curves, contours, clipping and dynamic dash sequences.
- Canvas path, point and mesh batches through a wrapper that expires with its pen.
- Native paths, images, material paints, shape fitting, font types and copied Skia paints.
- Offscreen graphics with explicit begin/end or a scoped draw callback; unclosed child frames close with their host frame.
- Retained composition guests identified by Python source location and optional loop index.
- Full frame pointer/key state, loop control, seeded pen noise and random streams, pure math, and copyable native Chance streams.
- Native brush tools and callbacks registered with the session callback lifetime.

Some source spellings use an adapter: stage metadata becomes decorator or
context arguments; alpha replacement uses color components; rectangle sorting
uses Python `min` and `max`; bitmap population uses `image.from_rgba`; and a
pattern tile's texture shader becomes `Tile.paint()`. The fixed scheduler used
by `bristle_current` is not bound: its model can advance from Python
`update(elapsed)` using `floor(elapsed * 60 + 1e-9)`, with the native scheduler's
eight-step catch-up limit and discarded excess backlog. Optional brush records
are copied when read; edits are assigned back to the tool.

## Verification boundaries

`test_pen.py` exercises real raster sessions: canvas expiry, clipping and batch
drawing, offscreen pixel retention across resize, reopening buffers under a
different host, exception cleanup, distinct retained guest call sites, color
models, dynamic dashes and seeded stream copies.
The value and brush contract suites exercise their own boundary types.

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
