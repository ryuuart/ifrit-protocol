# spell-circle-canvas docs

A jump table into this app's documentation and source. Start here, then
follow a link to the file that actually answers your question — this page
stays short on purpose.

## Contents

- [Architecture](#architecture)
- [TextFlow](#textflow)
  - [Header reference](#textflow-header-reference)
- [TextFlow gallery](#textflow-gallery)
- [TextFlow demo](#textflow-demo)
- [SigilCompose gallery](#sigilcompose-gallery)
- [Build & test](#build--test)

## Architecture

The receiver → model → renderer pipeline (`NetworkManager`, `SpellCircleModel`,
`SpellCircleRenderer`, the Skia/QCanvasPainter backends, `SyphonBridge`) is
described in the repository root [`CLAUDE.md`](../../CLAUDE.md#architecture).
That's the map for the Qt app itself; the sections below cover the
Qt-independent text engine and its two standalone executables.

## TextFlow

TextFlow is the Qt-independent Skia text layout library under `src/textflow/`.
[`src/textflow/README.md`](src/textflow/README.md) is the primary reference:
pipeline diagram, why it's fast, measured benchmarks, the full API sketch,
the accuracy/performance dial, tuning knobs, threading model, and known
limitations. Read that first for anything conceptual.

### TextFlow header reference

Each public header opens with a short "what is this / when do I reach for
it" banner; this table is just the index. All of it (plus every type, field,
and function) is written as Doxygen comments, so you can also browse it as
generated HTML:

```sh
cd src/textflow && doxygen Doxyfile   # -> ../../build/docs/textflow/html/index.html
```

| Header | What it's for |
|---|---|
| [`TextFlow.h`](src/textflow/include/textflow/TextFlow.h) | Umbrella include + a map of every other header below |
| [`Style.h`](src/textflow/include/textflow/Style.h) | `TextStyle` = `ShapingStyle` (cache key) + `PaintStyle` (draw-time) |
| [`PaintShaders.h`](src/textflow/include/textflow/PaintShaders.h) | Animated water, mesh-gradient, and tiled star `SkShader` presets |
| [`FontContext.h`](src/textflow/include/textflow/FontContext.h) | Per-thread service object: shape cache, HarfBuzz, ICU |
| [`Paragraph.h`](src/textflow/include/textflow/Paragraph.h) | The document model: text + style spans + placeholders, live-editable |
| [`Flow.h`](src/textflow/include/textflow/Flow.h) | The geometry text flows into: `BlockFlow`, `ExclusionFlow`, `VerticalBlockFlow`, `LineSetFlow`, `PathFlow` |
| [`ParagraphLayout.h`](src/textflow/include/textflow/ParagraphLayout.h) | `layoutParagraph()` and its options/result — the main entry point |
| [`Shaper.h`](src/textflow/include/textflow/Shaper.h) | Lower-level `ShapedWord` shaping types the pipeline is built on |
| [`Query.h`](src/textflow/include/textflow/Query.h) | Optional: find ranges by substring/word/regex, edit-following `MarkerSet` |
| [`Choreograph.h`](src/textflow/include/textflow/Choreograph.h) | Optional: per-glyph animation (`forEachPlacedGlyph`, `GlyphRSXformBatches`) |
| [`SingleLineParagraphCache.h`](src/textflow/include/textflow/SingleLineParagraphCache.h) | Optional: fast path for high-frequency labels/captions |
| [`textflowqt/TextFlowQt.h`](src/textflow/qt/include/textflowqt/TextFlowQt.h) | Optional: Qt bridging (`QFont`/`QString`/`QColor` conversions), zero-copy |

## TextFlow gallery

`TextFlowGallery` (`src/textflow/examples/gallery/`) is the interactive Qt Quick scene
switcher — live text editing, font/size/alignment/line-breaking controls,
a dynamic panel for every axis exposed by the selected variable font, and a
GPU/CPU raster A/B switch. Shared scene
helpers (palette, body-paragraph cache, caption drawing, filler text) live in
[`scenes/SceneSupport.h`](src/textflow/examples/gallery/src/scenes/SceneSupport.h). `makeScenes()`
in [`GalleryScenes.cpp`](src/textflow/examples/gallery/src/SceneRegistry.cpp) registers the scenes
below in switcher order:

| Scene | File | What it shows |
|---|---|---|
| Exclusions & shapes | [`ExclusionsScene.cpp`](src/textflow/examples/gallery/src/scenes/ExclusionsScene.cpp) | Moving circle/path exclusions, a live-morphing spiky ring, live text editing |
| Knuth–Plass & hyphens | [`KnuthPlassScene.cpp`](src/textflow/examples/gallery/src/scenes/KnuthPlassScene.cpp) | Greedy vs. optimal breaking side by side, soft hyphens |
| Infinite loop | [`LoopScene.cpp`](src/textflow/examples/gallery/src/scenes/LoopScene.cpp) | Text marquee around a closed figure-eight contour |
| Letter rain | [`RainScene.cpp`](src/textflow/examples/gallery/src/scenes/RainScene.cpp) | Full-paragraph relayout every frame while letters fall as particles |
| Ripple pool | [`RippleScene.cpp`](src/textflow/examples/gallery/src/scenes/RippleScene.cpp) | Click-to-drop expanding rings displace placed glyphs |
| Vertical CJK | [`VerticalScene.cpp`](src/textflow/examples/gallery/src/scenes/VerticalScene.cpp) | `vertical-rl`, ruby, kenten, tate-chu-yoko |
| Unicode singularity | [`HyperScriptsScene.cpp`](src/textflow/examples/gallery/src/scenes/HyperScriptsScene.cpp) | `﷽`, `𰻞`, `𒀱`, `ཧཱུྃ`, `꧅`, `᎙`, Zalgo stacks, bidi collisions, and emoji ZWJ sequences |
| Paint layers | [`EffectsScene.cpp`](src/textflow/examples/gallery/src/scenes/EffectsScene.cpp) | Animated water and mesh shaders, tiled stars, gradient composition, presets, and visible draw counts |
| 2,000-word shader stress | [`EffectsStressScene.cpp`](src/textflow/examples/gallery/src/scenes/EffectsStressScene.cpp) | A fully placed huge paragraph with four animated paint passes and no steady-state relayout |
| Query & markers | [`MarkersScene.cpp`](src/textflow/examples/gallery/src/scenes/MarkersScene.cpp) | Regex-found `MarkerSet` ranges that follow live edits |
| Inline slots & pills | [`SlotsScene.cpp`](src/textflow/examples/gallery/src/scenes/SlotsScene.cpp) | `appendPlaceholder()` pills and figures woven into the flow |
| Overflow & ellipsis | [`OverflowScene.cpp`](src/textflow/examples/gallery/src/scenes/OverflowScene.cpp) | CSS `text-overflow`-style clipping vs. shaped ellipsis |

## TextFlow demo

`textflow_demo` (`src/textflow/examples/demo/`) is the Qt-free reference: it renders each
scene to PNG under `textflow_demo_out/` and prints per-frame timing. Shared
helpers (palette, `TimingStats`, PNG output, filler text) live in
[`DemoSupport.h`](src/textflow/examples/demo/DemoSupport.h); [`textflow_demo.cpp`](src/textflow/examples/demo/textflow_demo.cpp)
is just `main()` calling the scenes below in order.

| Scene | File | What it shows |
|---|---|---|
| A — Exclusions | [`SceneExclusions.cpp`](src/textflow/examples/demo/SceneExclusions.cpp) | Mixed-script rich paragraph reflowing around moving shapes |
| B — Knuth-Plass | [`SceneKnuthPlass.cpp`](src/textflow/examples/demo/SceneKnuthPlass.cpp) | Justified paragraph with live word updates |
| C — Freeform | [`SceneFreeform.cpp`](src/textflow/examples/demo/SceneFreeform.cpp) | Spiral path + an arbitrary slanted line set |
| D — Extreme | [`SceneExtreme.cpp`](src/textflow/examples/demo/SceneExtreme.cpp) | Zigzag, scribble, bumpy baselines, rotated-letter confetti |
| E — Typography | [`SceneTypography.cpp`](src/textflow/examples/demo/SceneTypography.cpp) | Last-line modes, hyphenation, paint effects, mixed fonts |
| F — Pretext choreography | [`ScenePretext.cpp`](src/textflow/examples/demo/ScenePretext.cpp) (dispatches [rain](src/textflow/examples/demo/SceneRain.cpp), [ripple](src/textflow/examples/demo/SceneRipple.cpp), [loop](src/textflow/examples/demo/SceneLoop.cpp)) | Per-glyph choreography over a full relayout every frame |
| G — Babel & features | [`SceneBabel.cpp`](src/textflow/examples/demo/SceneBabel.cpp), [`SceneFeatures.cpp`](src/textflow/examples/demo/SceneFeatures.cpp) | Script-coverage confetti; OpenType feature rows |
| H — CJK | [`SceneCjk.cpp`](src/textflow/examples/demo/SceneCjk.cpp) | Vertical-rl, ruby, kenten, tate-chu-yoko |
| I — Shapes | [`SceneShapes.cpp`](src/textflow/examples/demo/SceneShapes.cpp) | Star/heart/donut `SkPath` exclusions |
| J — CJK fallback | [`SceneFallback.cpp`](src/textflow/examples/demo/SceneFallback.cpp) | Platform fallback resolves Japanese, Korean, and Chinese glyphs under two incomplete primary families |
| K — Unicode singularity | [`SceneHyperScripts.cpp`](src/textflow/examples/demo/SceneHyperScripts.cpp) | Arabic calligraphy, rare glyphs (`𰻞 𒀱 ཧཱུྃ ꧅ ᎙`), combining storms, bidi, and emoji in one torture wall |
| L — Paint layers | [`ScenePaintEffects.cpp`](src/textflow/examples/demo/ScenePaintEffects.cpp) | Water/mesh/star/gradient composition plus a fully placed 2,500-word, four-pass stress image (`paint_stress.png`) |

## SigilCompose gallery

`ComposeGallery` exercises the retained composition engine across all scenes in
`src/common/compose/gallery/`. Its interactive view renders directly into a Qt
Quick texture through Skia Graphite on supported Metal backends; the portable
QRhi fallback rasterizes once and performs one explicit texture upload. The
interactive view requires a hardware-backed Qt Quick RHI; `--headless` remains
available in software-only environments.
Use a Release build for performance work—Skia effect-heavy scenes are
intentionally stressful and Debug timings are not representative.

```sh
cmake --build build --config Release --target ComposeGallery compose_bench
./build/bin/Release/ComposeGallery
./build/bin/Release/ComposeGallery --headless /tmp/compose-gallery
ctest --test-dir build -C Release -R 'compose(_gallery)?_test'
```

## Build & test

See the root [`CLAUDE.md`](../../CLAUDE.md#build-and-test) for the full
setup/build/test commands. Quick reference for the executables covered above:

```sh
cmake --build build --config Release --target textflow_bench textflow_demo TextFlowGallery
./build/bin/Release/textflow_demo          # writes PNGs + per-frame timings
./build/bin/Release/TextFlowGallery        # interactive scene switcher
ctest --test-dir build -C Debug -R textflow_test
```
