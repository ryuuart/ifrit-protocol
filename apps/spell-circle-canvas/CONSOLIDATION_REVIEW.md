# Consolidation review

The strongest organizing model is **props + children → a drawing**, with
layout, geometry, motion and paint providing independent operations beneath
it. A kit chooses useful defaults and composes those operations. A sketch
chooses the subject, palette, type and choreography.

This is an architecture and authoring-surface audit of the current tree,
including recent refactors, feature targets, public headers, kits, host
responsibilities and representative tests and benchmarks. It is not a
line-by-line correctness certification of every implementation.

## Concrete consolidation

- Track layout belongs to Compose core. `core/Grid.h` exposes the values
  and contract; `core/Grid.cpp` owns area parsing, track sizing and placement.
  The public header is 155 lines instead of carrying the entire solver.
- Equal modules use repeated fractional tracks in `layouts::Grid`.
  `ModularGrid` and its parallel span list are removed. Children own their
  placement, and automatically placed children avoid claimed cells.
- Compose owns the shared cell-run and panel-grid arrangements. Both accept
  arbitrary Element children and concrete spacing, alignment and divider
  props. The Sketch wrappers only resolve theme defaults.
- Sketchbook commands live in `SketchActions`; the catalog owns rows and
  thumbnails. Commands receive a row value, independent of browser selection.
  Video export requests the device-capable path, which opens a device only
  when the selected runtime needs one. Process launch failures reach status.
- The shape-identity benchmark uses identical star geometry in both arms.
  Comparable values versus raw callables is its only intended variable.
- Path placement reads closure from the selected contour. A later contour
  cannot change whether the first contour includes its endpoint.

These are breaking simplifications. There is no compatibility alias for the
removed grid or its include path.

## Decisions and completed fixes

| Review item | Decision |
| --- | --- |
| Size-dependent decoration bounds | Implemented. Decorations can query overflow and mark width with the resolved size. Brushes, weaves, edge slices and inset wrappers forward it. Gel presets no longer ask callers to guess a maximum height. Shadow bounds use Skia's actual filter support. |
| Fill versus material component props | Implemented as Compose's `SurfacePaint`. Wells, sheets, sketch components and stroke formatting accept the same value. Live fill bindings and material identity survive composition. The Sketch-only Ground type and PathFormat's separate material slot are removed. |
| Painting machinery in era presets | Both body ramps now use the existing Wash primitive. Keep the lens geometry and horizon-band arrangement in the presets: they express different looks, and extracting a configurable renderer for those two small arrangements would add a second authoring model. |
| Sketchbook responsibilities | Commands are in SketchActions; CanvasPane owns pan/zoom, orbit, input and error presentation through explicit properties and events. Keep device capture/readback on the render thread because it shares the context and image lifetime of the presented session. |
| Catalog metadata and thumbnails | SourceMetadata and its parser live beside source discovery, without Qt. Keep ThumbnailQueue as the one worker service; the catalog already marshals its reports into rows. Keep process startup configuration for this single-window host instead of adding a service container or a second registry. |
| Theme dependence in deferred descriptions | Keep explicit value capture, which is already the documented rule. The audit found no deferred theme read requiring an inherited context cache. Theme changes belong in comparable component props; no implicit theme cache is introduced. |
| Almost-identical material header names | Implemented: Pbr.h holds metallic-roughness surfaces; Reflections.h holds the normal/environment-map looks. Their distinct shading algorithms remain separate. |
| Large public algorithms and authoring verbosity | Grid's solver and shared cell arrangements are out of line; source metadata parsing is separated from the browser. Keep templates and small prop builders visible. surface_components is a compact props-and-children example rather than another specialized kit. |
| Tests and benchmarks | Keep distinct behavioral contracts and meaningful comparison dimensions. Remove coverage for deleted vocabulary and fix the incomparable benchmark arms; do not choose an arbitrary test-count target. |
| Feature demonstrations | COVERAGE.md maps the public visual feature families to actual examples and names the infrastructure, option combinations and device lanes that pictures cannot certify. All existing sketches remain; surface_components adds a focused example for the generalized surface props and resizing styles. |

Additional defects fixed during the pass:

- Host crashes no longer claim that a sketch image caused the fault. Reports
  name the recorded operation and stack, without guessing the cause.
- The crash-reporting test's pipe read is polled, so a child that hangs
  cannot block the parent indefinitely before its timeout.
- Live thumbnail notifications reach the catalog, updating the browser row.
- Capture numbering continues beyond 9,999 instead of reusing that filename.
- Shared Qt controls choose an installed monospace family rather than
  asking Qt for a nonexistent comma-separated family name.

These decisions close the review's action list. They do not claim that every
possible future enhancement or every combination of library options has
been exhausted.

## Boundaries that should remain

| Libraries or features | Why they are distinct |
| --- | --- |
| Compose / Draw | Retained descriptions and imperative pen commands are different authoring modes. They should interoperate through the existing adapters. |
| World / Geometry | Scene/pass orchestration differs from paths, meshes, cameras and point operations. CPU and device implementations should meet at the same executor seam. |
| Image / IO / Material texture | Decoded image meaning, resource acquisition, and sampling/material inputs are separate responsibilities. Share resource identity and decoding rather than merging their APIs. |
| Motion schedule / Core schedule | Choreographic timing differs from task execution. Similar directory names are not duplicated algorithms. |
| Weave / Compose typography | Paragraph shaping and layout differ from attaching text to a retained drawing and animating its units. |
| Table / Grid | Automatic table sizing has different surplus and row-span rules from track sizing. Share cell-flow mechanics; keep those behavioral contracts explicit. |
| Measure / Compose testing | Measurement and result data differ from inspecting a rendered scene. A rendered check panel should consume result values. |

The number of CMake targets alone is not a reason to merge libraries.
Optional SDKs and device executors justify separate link boundaries. The
more useful reduction is in how many concepts an author must choose among
to express one operation.

## Tests and benchmarks

The source inventory is approximately 3,100 GoogleTest case declarations
and 473 benchmark registrations. Registrations expand across arguments;
these counts are not the number of independently maintained benchmark
programs. The build already aggregates one test binary and one benchmark
binary per library.

Use the following retention rule:

- Keep a test when it distinguishes an observable contract: child identity,
  clipping, a layout boundary, cache invalidation, resource lifetime or CPU
  versus device agreement.
- Put sizing arithmetic tests beside the owning solver. Keep one or a few
  rendered integration cases proving that its output reaches real nodes.
- Keep themed-kit tests about default resolution, override behavior and
  component composition. They should not repeat every solver case.
- Delete tests whose only distinction is a removed spelling or an obsolete
  implementation structure. Repeated cases with identical setup and varying
  data belong in a parameterized case or a short data loop when failure
  diagnostics stay clear.
- Keep benchmark dimensions that answer a question: cold versus warm,
  static versus changing, node count, image size or backend. Remove arms
  that cannot affect a decision, and make comparison arms use identical
  content and configuration.
- Do not run the entire catalog between local edits. Use one final
  verification pass over the changed contracts and representative images;
  reserve the full integration lanes for their actual integration point.

There is insufficient evidence for a blanket deletion of thousands of
tests. The clearest excess is repeated setup and explanatory prose, while
many apparently similar cases distinguish real cache or rendering behavior.
A systematic next reduction should group tests by the contract they protect,
not by filename or a desired count.

## Authoring rules

1. A component takes named props and arbitrary children or meaningful slots.
2. Placement lives on the child or in the parent's layout; never in a
   parallel list that must stay synchronized with sibling order.
3. A preset chooses values; it does not invent its own layout or renderer.
4. Units are explicit. A duration, normalized progress, angle and pixel
   distance should not share an unexplained float convention.
5. State and frame stepping belong to the host. Describing a component
   should read inputs and return a value.
6. A new reusable operation must simplify more than one distinct example.
   Keep reference-specific typography and composition near the sketch.

## Verification

- Release builds pass for the affected libraries, all three test and
  benchmark binaries, standalone Compose headers and Sketchbook with its
  complete catalog.
- All 1,533 library cases pass: 1,042 Compose, 206 Sketch and 285 Material.
  Metal cases run with device access; the web-engine case runs in its own
  process because the SDK permits one engine per process.
- The catalog contains 196 sketches: 147 canvas, 34 draw and 15 set. Every
  previous key remains. All 119 examples linked by COVERAGE.md exist and
  belong to the registry. The catalog-row integration test passes.
- Changed C++ and QML pass the formatting/lint check. The final diff has no
  whitespace errors.
- surface_components renders headlessly, compiles live and opens in the
  refactored window. The final window log has no QML or font-family errors.
- grid_layouts and border_weave remain pixel-identical to their preceding
  renders. chrome_type exposes the shadow extent that caching previously
  clipped; its resulting plate was visually inspected.

Full catalog plate tiers, video export, timed benchmark sweeps and sanitizer
lanes were not run. Compiling their targets does not claim those lanes were
run. The findings queue is empty; each architecture decision above has an
explicit disposition.
