# Python authoring coverage

The coverage target is every capability needed to reproduce every native sketch
in Python, including the creative libraries and their kits. Raw bindings and
Python conveniences must share native values. Named parameters, precise editor
types, ordinary Python collections and owned results (values Python holds
independently of the native call or session that produced them) are part of
coverage.

The package does not yet meet that target. This ledger records what each
library binds, which native requirements stand between each registered C++
sketch and a Python translation, and what the tests and studies verify. A bound
operation is not a tested sketch port, and a module name is not evidence of
full library coverage. Everything listed as not yet bound is implementation
work, not a permanent exclusion.

## Status

A sketch counts as bound when a translation needs only bound operations, the
adapters under "Translating a C++ source" and ordinary Python code. That makes
a translation possible; it does not mean one exists or that its pixels match
the original. The Python study column counts the bundled studies that port or
adapt a sketch in that collection; the other studies have no native
counterpart.

| Collection | Sketches | Every requirement bound | Python study |
| --- | ---: | ---: | ---: |
| Start & fixtures | 5 | 1 | 0 |
| Draw | 3 | 3 | 0 |
| Draw · Generative | 5 | 5 | 2 |
| Draw · Observable reproductions | 15 | 15 | 4 |
| Draw · Procedural | 8 | 8 | 1 |
| Kit · API | 73 | 3 | 0 |
| Kit · Depth | 1 | 1 | 0 |
| Compose · Typography | 1 | 1 | 1 |
| Specimen | 13 | 0 | 0 |
| Set | 11 | 0 | 0 |
| Data | 14 | 0 | 0 |
| Media | 5 | 0 | 0 |
| Catalog · Type | 10 | 1 | 0 |
| Catalog · Chrome | 2 | 1 | 0 |
| Catalog · Game UI | 5 | 0 | 0 |
| Catalog · Generative | 2 | 0 | 0 |
| Catalog · Tiling | 1 | 0 | 0 |
| Study · Type | 6 | 0 | 0 |
| Study · Pattern | 4 | 0 | 0 |
| Study · Motion | 4 | 0 | 0 |
| Study · Film | 4 | 0 | 0 |
| Study · Science | 5 | 0 | 0 |
| Study · Esoteric | 3 | 0 | 0 |
| Study · Screens | 6 | 0 | 0 |
| Study · Game UI | 9 | 0 | 0 |
| **All** | **215** | **39** | **8** |

Every Draw sketch is bound. No set sketch is, because Python sketches cannot
yet host sets: `lantern_room` uses only bound World verbs and waits on set
hosting alone.

These surfaces stand between the most sketches and a translation. Each is
needed by eight or more; the catalog audit names every surface each sketch
needs.

| Not yet bound | Library | Sketches |
| --- | --- | ---: |
| `geometry::shapes` silhouette generators | Geometry | 54 |
| `sketch::kit::sectionHeader` | Sketch kit | 44 |
| `motion::Spread` and `motion::Cascade` schedules | Motion | 23 |
| `SkSurfaces::Raster` standalone raster surfaces | Skia | 21 |
| `compose::Element::fx` text tracks | Compose | 21 |
| `compose::instancing` atlases, pools and instances | Compose | 18 |
| `compose::keyedShape` comparable shapes | Compose | 18 |
| `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`) | Compose kit | 17 |
| `material::pattern` stock tiles | Material | 17 |
| Set sketches (`sketch::SetContext`, `describe`) | Sketch host | 16 |
| `geometry::shapes` corner operators | Geometry | 15 |
| `compose::Element::mask` and `compose::by` gates | Compose | 14 |
| `compose::lines::presets` and `compose::brush::presets` | Compose kit | 14 |
| `sketch::kit::readout` | Sketch kit | 14 |
| `available(why)` probes and `sketch::requireCached` | Sketch host | 13 |
| `data::Connection` | Data | 13 |
| `geometry::mesh::pop` point-operator chains | Geometry | 13 |
| `material::Texture` and `Material::slot` | Material | 13 |
| `sketch::kit::plot` chart layers | Sketch kit | 13 |
| `measure::CheckTable` and `measure::check` | Measure | 12 |
| `sketch::kit::instrument` | Sketch kit | 12 |
| `sketch::kit::Document` content reader | Sketch kit | 11 |
| `sketch::kit::table` | Sketch kit | 11 |
| `compose::lines::Line` and `lines::Rails` | Compose | 10 |
| `compose::styles` layer styles (glow, inner shadow, bevel) | Compose | 10 |
| `geometry::mesh::Cloud` and `mesh::points` | Geometry | 10 |
| `geometry::shapes` parametric curve generators | Geometry | 10 |
| `compose::TextureScene` and `SketchContext::textureScene` | Compose | 9 |
| `compose::onEdges` and `compose::inset` adaptors | Compose | 9 |
| `geometry::path::Frame` and `path::Grid` | Geometry | 9 |
| `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`) | Material | 9 |
| `material::sdf` shapes and styles | Material | 9 |
| `motion::Ticker::timeline` and standalone tickers | Motion | 9 |
| `compose::Border` and `compose::decorations::border` rules | Compose | 8 |
| `compose::connector`, `rail` and routers | Compose | 8 |
| `compose::fx` text effects (`keys`, `scramble`, `sequence`) | Compose | 8 |
| `geometry::mesh::curve::Spline3` and frames | Geometry | 8 |
| `material::Recipe` authoring | Material | 8 |
| `material::skia::Paint` motion-driven uniforms and offsets | Material | 8 |

## Library coverage

Each row states what a Python author can reach today and what the native
library has that Python cannot yet reach. Names under Bound are spelled as
Python reaches them; names under Not yet bound are native C++ spellings.

### Hosting and runtime

| Surface | Bound | Not yet bound |
| --- | --- | --- |
| Canvas sketches | `@sketch` classes, whose `setup`, `update` and `draw` may omit trailing arguments of `setup(ctx)`, `update(elapsed, ctx)` and `draw(pen, ctx)`, and which the catalogue and external workspaces discover; undecorated classes with `setup` and an optional `update`, which open only from their file path; a checked context (`canvas`, `background`, `captureAt`, `oversample`, `plate`, `nonlinearPicture`, `render`, `measure`, `snapshot`, `measured`, `local` and the session readings) that rejects use after teardown or from another thread; checked assets (`image`, `json`, `table`, `database`, `hub`, `root`); a fresh import of the source and its local modules each time a sketch opens; catalogue metadata read from docstrings, `TAGS` and `REQUIRES`; `render_file` through the native host | Set sketches: `sketch::SetContext` and `describe(seconds)` returning a `world::Frame`; `SketchContext::textureScene`, `bakeSet`, `canvas(CanvasSpecification)`, `fonts` and `key`; `Assets::video`; `sketch::Guest`; `painterRuntime` and `device`; `requireCached` and a declared `available(why)` probe; a registered name or category other than the file stem and `Python`; the per-sketch `<stem>.fbs` schema rule, which runs only for C++ entries; `sketch::scry` settling |
| Composer | Checked `ctx.composer`: `render`, `renderSlot`, `bounds`, `hitTest`, `routesAt`, `settling`, `active`, `dirty`, `purgeCaches` and owned `stats` | `paragraphLayout`, `beatsOf`, `units` and `cascadeSpanMs`; `setInherited`, `setView` and `declareInputSpace`; `setProfiling` with `profile`, `setCompositeCounting`, `setAutoTexturePromotion` and `setBakeDensity`; a standalone `compose::Composer` driven through `setSize`, `setClock`, `draw`, `setPointer` and `setKey` |
| Ticker | Checked `ctx.ticker`: `add` with a callback of zero to two arguments that returns `False` to leave, `addFixed` with its catch-up limit, interpolation output and `FixedStatus`, `derive`, `elapsed` and `active`; a World scene steps its own ticker through `Scene.advance` | A standalone `motion::Ticker` with `tick`; `Ticker::timeline()` over choreograph; `motion::FrameClock` |
| Memo | `compose.memo(properties, describe)`: a deep-copied model compared with Python equality, native reconciliation, the inherited environment restored around the deferred builder, and builder failures reported with a traceback | `copy.deepcopy` on bound native values, which every bound value type except the enumerations refuses, so a model cannot hold a color, `Theme`, `Fill`, `weave.Type`, `Element`, `Output`, `Table`, `Material`, `Mesh` or any other bound value; Python values in `core::environment` (`Provide`, `inherited`, `inheritedOr`, `bound`), so a Python kit's own theme is not a captured dependency |

Reloading replaces instance state and restarts the scene clock, for C++ and
Python sessions alike. A memo builder must be a pure function of its model and
inherited environment in both languages. Each Sketchbook process uses one
Python environment.

### Composition

| Surface | Bound | Not yet bound |
| --- | --- | --- |
| Elements | Factories `box`, `stack`, `positioned`, `text` over a string, a styled string or rich text, `frame`, `image` over a Skia image, `picture`, `pathFigure`, `slot`, `memo`, keyed `pen` and `graphics` with Python programs, and `layout` over the six stock schemes; every native Element verb except the five named opposite, with named arguments except `padding` and `margin`, which take one, two or four positional lengths: layout and placement, 2D and 3D transforms, fills, corners, shapes and clips, effects, blending and hit testing, opacity, entrances and transitions, caching, the cascade verbs `font`, `block`, `ink`, `styleSheet`, `styleClass`, `role` and `var`, and the text-leaf verbs; dimensions in px, pct, em, rem, lh, var, pw and ph; children as elements or one iterable | `Element::fx` tracks and the `fx::` text effects (`scramble`, `keys`, `hold`, `sequence`, `mix` and `variableAxis`), `mask`, `tether`, `variationDrive` and `mark`; keyed `shape(key, function)`; keyless `pen(program)` and `graphics(program)`; `custom` paint programs and pen programs that read `PaintContext`; `image(ImageAsset)`; `text(Paragraph)`; `connector`, `rail` and `band` with `Anchor`, `Router` and `RailRouter`; `feed`; `instancing` pools and atlases; `Pattern`; `Table` and Python layout schemes over `LayoutInput`; shader fills and gradient helpers; standalone `intrinsicSize`, `snapshot`, `metrics`, `measureRun`, `runPens`, `atCapHeight` and `fitRun`; `tiles` and `shelve`; `video` and `web` leaves; `TextureScene` and `texture`, which paint a tree into a material texture |
| Decorations | `Decoration` over `PathFormat` (stroke fill, alignment, dashes, caps, joins, stamps, trims and animated phases) or `Shadow`; `stroke`, `shadow`, `LayerStyle` and `Element.style`; overlay, background, foreground and stroke layers; span passes over `spans` selections; `echo`, `boundary` and `threshold` | `Slice`, `ContourWalk`, `Wash`, `Border` and the `decorations::` borders and washes; `onEdges` and `inset`; the `brush::` solids, weaves, scatters, patterns, corner art, ribbons and art; `Brush` shaped layer stacks and `brush::restyle`; `LayeredBrush`; `lines::Line`, `Rails`, `Hatch` and `RadialHatch`; the `styles::` inner shadow, outer glow, bevel and emboss, overlays, ripple, brackets, tick rails, scanlines, stipple and dither; `PathFormat::effect`; shadow offsets driven by motion outputs; decoration schemes written in Python |
| Compose kit | `compose.kit` records `Sheet`, `Panel`, `Board`, `Well`, `WellContent`, `Caption`, `Cells`, `PanelGrid`, `Line` and `Ladder` with every native field, their factories, `centred`, `at`, `disc`, `dot`, `ring` and `figure`, with caption, sheet and panel lines as Python callables; `compose.layouts` `Grid` with tracks, areas and dense flow, `Radial`, `Diagonal`, `BaselineGrid`, `Jittered` and `AlongPath` | The rows (`section`, `readout`, `table`, `bars`); typesetting (`ruby`, `kenten`, `bullets`, `columns`, `rules`, `nestedRun`); `annotate` with `Beside` and `Anchored`; the kinetic `fx::` presets, `trackMeter` and `restGhost`; `marquee`; legibility (`scrim`, `haloed`, `shaded`, `emboldened` and `drawHaloed`); `vignette` and `grained`; the chrome, gel and gloss styles; `plate`, `console` and `tinted`; `routers::`; `instancing::place::`; the stroke and brush presets; sprites and pixel type; `ornament::` and `flourish::`; the named default leaves |
| Document kit | Every `compose::document` component and property: `article`, `section`, `list`, `quote`, `heading` and `h1` to `h6`, `paragraph` over a string or rich text, `lead`, `caption`, `label`, `eyebrow`, `footer`, `code`, `item`, `figure` and `rule`; `measure`, `gap`, `listGap` and `quoteInset`; roles resolved through inherited stylesheets | Nothing in the native document kit. Native role rules match exact role and class names; the native library has no selector strings for Python to bind |
| Specimen kit | `sketch.kit` `Palette`, `Register`, `TypeScale`, `Spacing` and a comparable `Theme` with `font`, `style`, `styleSheet` and `voice`; `houseFace`, `houseTheme`, `studyTheme` and `theme`; a checked `Provide`; `stage`, `page`, `well`, `caption`, `cell`, `cells`, `panelGrid` and `comparison` | `specimenTheme`, `Theme::sans` and `Theme::mono`; `stage(SetContext)`; `passage`; `Document`; `Instrument`; the headings `titleCard` and `sectionHeader`; the rows `readout`, `labelRow`, `table` and `bars`; the legends `legend`, `swatchStrip` and `chip`; `meter` and `gauge`; the charts `plot`, `axis`, `rules`, `trace`, `area`, `path`, `marks`, `bands`, `segments` and `label`; `scrollbar`; `backdrop`, `panel` and `frame`; `console`; `ticker` and `timeline`; `Channel` |

### Typography

| Surface | Bound | Not yet bound |
| --- | --- | --- |
| Retained typography | `Type` and `TextStyle` with em, rem and lh lengths; `PaintStyle`, `PaintLayer`, `Decoration` and the kit shadow, glow and outline layers; `Block` and `ParagraphStyle` with leading, justification, hyphenation limits, tabs, indents, keeps, initial letters, reserved bands and the kinsoku, hanging and mojikumi tables; `FrameOptions` through `ParagraphLayoutOptions.frame` and the Element verbs `firstBaseline` and `distribute`; `TypeSheet` and `StyleSheet`; `RichText` with slots; `Story`; `weave.selectors` and Compose `selectors`; `TextPath`; Compose `Annotation`; vertical writing through `Block.writingMode` and `Type.verticalForm` | `weave::overlay`, `merge`, `reshapes` and a stated `defaultFace`; `HyphenationOptions::patterns` on a block; `weave::paint::setMaterialResolver`, without which a paint layer's material draws unshaded in every host. |
| Paragraph engine | `Paragraph` editing and inspection over UTF-16 ranges; `ParagraphBuilder`; a thread-checked `FontContext` over the system font manager; `BlockFlow`, `VerticalBlockFlow`, `PathFlow`, `LineSetFlow` and `ExclusionFlow` with `silhouette` shapes; `layoutParagraph`, `layoutSingleLine` and their options; a `ParagraphLayout` with drawing, batched drawing, line and column metrics and glyph outlines, which refuses use once its paragraph changes; `findAllOccurrences`, `findRegexMatches`, `wordRanges` and `MarkerSet`; `layoutBeside`, `warichuSplit` and `layoutWarichu` | Python subclasses of `FlowGeometry`, `Silhouette` and `Hyphenator`; `FontContext` over a supplied font manager with a fallback resolver; `ports::face` family chains and width styles; `TextContext` and `TextLayout`; shaped words (`shapeWord`, `faceMetrics`, `lineHeightOf`, `Paragraph::words`); `ParagraphLayout::placeholderRects`; `LineInterval::contour` and `placeAt`; `Selector::state`; the `unicode::` leaf |
| Glyph choreography | Nothing | `ParagraphLayout::runs`, `PlacedGlyph` and `forEachPlacedGlyph`; `GlyphDress`; `GlyphRSXformBatches`; live variation drawing; `kit::GlyphBuckets` and `drawPositionedGlyphs` |
| Weave kit | `PatternHyphenator` with the English patterns; `dropShadow`, `glow` and `outline` | The `features::` presets and `stylisticSet`; `kinsoku::japanese`, `hanging::latin` and `hanging::japanese`; `makeStyle`, `tracked` and `drawLabel`; `LayoutGuard`; `mixedScriptFiller` |

A native defect reaches Python through `Annotation`: one group-ruby reading
over a compound that word units divide, or that breaks across columns, repeats
over every piece.

### Motion

| Surface | Bound | Not yet bound |
| --- | --- | --- |
| Values and bindings | Shared `Output`, `ColorOutput` and `FillOutput`; `bind` and every `Bound` stage; `wiggle`; `ease` curves and Python callables; `Transition`, `ramp`, `animate` over `from_`, `to` and `through`, and `entrance` and `transition` for floats, colors and fills, with read-back; the float `Animatable`; `phase`, `quantizeTime`, `stepIndex`, `decay`, `flash` and `clamp01`. Times are seconds, truncated to whole milliseconds | `Oscillator` and `Wave`; `Sequence`; `Spring` with `spring` and `springMoving`; `AnimatedFloat` and held-motion resolution; `Lane` with `retargetSlots` and `retargetFamily`; `BoundFloat`, `Envelope` and `Bound::value`; the color and fill `Animatable` and its accessors; curve shape read-back, without which a Python-authored curve compares unequal |
| Schedule | `Element.staggerChildren(seconds, from_)` with `from_` of `"start"`, `"center"` or `"end"` | `Spread` with its random and edge origins; `cascadeOrder`, `cascadeRanks`, `Cascade` and `Beat`; World `Element::staggerChildren` |
| Physics | Nothing | `motion::physics`: `Points`, forces, flocking, `Neighbourhood`, constraints, `Verlet`, `Particles` and `Emitter` |

### Drawing

| Surface | Bound | Not yet bound |
| --- | --- | --- |
| Pen | `draw.Pen`, checked for its drawing thread and callback: every p5 constant and blend mode; color models; geometry, curves, shapes and contours; clipping and dashes; `shape` over a path or a Python silhouette; `vertices`; text, with `textFont` over a family, `weave.Type` or a typeface; images; transforms; seeded random, Gaussian and noise streams and `NoiseField`; `map`, `lerp`, `constrain`, `dist`, `mag`, `norm`, `sq`, `radians` and `degrees`; material fills and strokes fitted to the canvas or the shape; inherited ink and font; frame input and loop control; `element` guests keyed by source location and loop index | A standalone `Pen` with `begin` and `end`; `Pen::retained()` state; declarations for `fill` and `stroke` over a paint with `CANVAS` or `SHAPE`, over a `Material`, and `background` over a paint, which run but fail strict type checking; a silhouette type for `shape` in place of the declared `skia.Path` |
| Canvas and offscreen buffers | `pen.canvas()`, expiring with its pen: save and restore, transforms, clips, `drawPath`, `drawRect`, `drawCircle`, `drawPoints` over iterables or float32 buffers, and `drawVertices`; `draw.on`; `draw.Graphics` with explicit or scoped frames, a `resize` that keeps pixels, and frames that close with their host frame | `drawLine`, `drawOval`, `drawArc`, `drawRRect`, `drawPaint`, `drawImage`, `drawImageRect`, `drawPicture`, `drawTextBlob`, `saveLayer` and `concat`; an owned raster surface for drawing before the first frame or outside a host pen |
| Brush | `draw.brush`: tool, pressure, curve, response, dynamics, shape, grain and input records, with Python callables where native takes functions; the stock tools; strokes, sampling and deposition; fields over Python callables; washes, hatches and masses; polygons and plots; catalogues and the engine. Python curves, fields and tips follow the host callback lifetime | `weightedChoice`; `kSpeedFilterSeconds`; `copy.deepcopy` for brush values |
| Brush formats | `draw.brush.format`: `encodeBrush`, `decodeBrush`, `assembleBrush`, and Photoshop and Procreate brush decoding | `loadBrush`; a brush decoder registered with a hub |
| Skia values | `skia` points, rectangles, sizes, colors, matrices, paths and path builders, `pathOp`, paint setters, blend, tile, filter and sampling modes, `Vertices`, `Image` with RGBA read-back, `Picture`, `RuntimeEffect.MakeForShader` and `Typeface` with its family name | Paint shaders, color and image filters, mask filters, path effects and blenders; `SkShaders` and gradients; `SkRRect`, `SkM44` and most of `SkMatrix`; `SkPathBuilder::arcTo`; path queries beyond `contains`, `getBounds` and `isEmpty`, SVG path strings, `Simplify`, `skpathutils::FillPathWithPaint` and `SkContourMeasure`; `SkTypeface` variation axes and style read-back; `SkFont` and `SkTextBlob`; bitmaps, pixmaps and image info; the SigilSkia library itself: `draw::drawLattice`, `draw::drawSpriteAtlas`, the float and half-float pixel helpers and the Graphite bring-up |
| Image | `image.decode`, `decodeAsset` with animated `ImageAsset.frameAt`, `encode` to PNG, JPEG, WebP and EXR, `from_rgba`, `load` and `save`; hub and session image loading | `DecodeOptions::layer`; `probeImage`; `decodeChannels`; pixmap and channel encoding with 16-bit and float pixels; `ImageAsset::frames`, `repetitionCount` and `wrap`; `coverageMask` and `distanceField`; `formatForPath` and `extensionFor`; `embeddedPngs`; reporting which codecs are available |
| Core | `core.chance` sources and streams, with copies; curves as `motion.Curve`; the inherited environment through memo and `sketch.kit.Provide` | `Stream::sample`, `reseed` and `seed`; the distributions, `shuffle` and `Reservoir`; `noise::Field` and the noise kinds; the `noise::` mixers and lattice words; `hash::fnv1a` and `combine`; interval normal forms |

### Material, geometry and World

| Surface | Bound | Not yet bound |
| --- | --- | --- |
| Material | Colors with OKLab, OKLCH, CIELAB and linear conversions, gamut fitting and difference; ramps, palettes and extraction, harmonies and dither; the `field` recipes `noise`, `grain`, `ripple`, `halftoneRamp` and `crtOverlay`; `kit.SurfaceParameters` with `surface` and `unlit`; `pattern.Tile` grid lines, stripes, checker and halftone; `Paint` (solid, gradients, image, SkSL, recipe and blends, with uniforms, slots, fitting and world space) and `Effect` (glow, bloom, blurs, the light stages `dilate`, `deepen` and `whiten` joined by `emit`, shaders, slots and uniforms driven by motion outputs) and `bloom` over `BloomParameters` | `Material` equality and editing; `Recipe` authoring with frame inputs, `UniformBlock` and banks; `over` stacks and masks; `Texture`, texture discovery, `Atlas`, `EnvironmentMap` and `bevelNormals`; `sdf::`, `ocio::` and `slang::`; `pattern::sequence`, `speckle`, tile textures and filters, and weaving; `skia::fill`, `shader`, `verticalRamp`, `unitRamp` and palette images; `PixelBuffer` with `Paint::buffer`; `Paint::shader`; paint uniforms driven by motion outputs and `Paint::offset`; `Paint::image` sampling options; `Effect::filter`; the kit's ramps, grained surfaces, globe, girih, reflection materials (`gold`, `chrome` and `glass` over normals and an environment map), environments, text paints and texture-mapped surfaces |
| Geometry | `geometry.arrange`; `mesh.Mesh` and its generators `grid` over a Python function, `quad`, `box`, `platonic`, `torus`, `superellipsoid`, `cylinderPanel`, `extrude` and `revolve`; `mesh.camera` cameras, orbits and placement; `mesh.render` lights and `MeshStyle` with `drawMesh` and `drawImagePanel` on the CPU executor | `geometry::path`: polylines, contours and poses, operations and distortions, lattices, regions and sampling, triangulations and hulls, streamlines, symmetry, frames and grids, profiles and bands, crossings, blends, conic sections and spherical projections; the `shapes`, `shapers` and `sections` kit; mesh primitives, faces and extrusion caps; `Camera::view`, `projection`, `viewProjection`, `clipProjection` and `extentAt`, and element access on `mesh.camera.Matrix`, which `Scene.transformOf` returns; `drawPanel`, render environments and runtimes; `mesh::curve` splines and frames; point clouds, `mesh::points` and the `mesh::pop` operators and sweeps; `mesh::codec`; `geometry::device` |
| World | Keyed `world.Element` with transforms, meshes, fills, lights, cameras, tags and animatable lanes; selectors; geometry and post passes; `Frame`; a thread-checked `Scene` that owns its ticker, with `render`, `advance`, `image`, `draw(pen)`, handles and statistics; `light` sun, point and spot; the `kit` rig, turntable and lit set; the CPU executor | Environment lighting through `environmentMap`, `crossfade` and `backdrop`; `along` with `kit::rail`, `wave` and `winding`; `cloud`, `chain`, `stamp`, `window` and `generate`; `cache`, `staggerChildren` and `world::memo`; `computePass` and pass bodies; `readback`; `Scene::plan` and `targets`; the device runtimes and `importNative`; `light::directional`; selector inspection and `Element::node` tree read-back; `Pass` equality; a scene over the session ticker; texture-dressed surfaces |

The CPU executor shades a World surface from base color, lighting and metallic
response. Roughness changes only the reflection of an environment map, which
Python cannot supply yet, and a surface's emission (`emissive`), transmission
and texture maps do not reach a World pixel. An image set as
`mesh.render.MeshStyle.texture` does texture a mesh drawn with `drawMesh`.

### Data and IO

| Surface | Bound | Not yet bound |
| --- | --- | --- |
| Data and assets | `Json`, `Column` and `Table` reshaping, `decodeCsv`, `Instant` and `Flag`; `Interval` and `Scale`; SQLite and DuckDB `Database` values with owned writes and query views; `Schema` text and binary conversion; OSC, MIDI and Art-Net codecs; `registerDecoders`; the session asset loaders | `data::Connection`; `FlatBuffer` roots and the generated value types; a database decoder on a standalone hub; typed column spans; `Instant` ordering and `Flag` equality and ordering; `maxOscBundleDepth` |
| IO | An owned or session `Hub` with mounts, resolution, text, bytes, probes, selection, writes, polling, typed `load` for tables, JSON, databases and image assets, leases, preload and network policy; `Feed` with `receive`, `newest`, `latest`, status, `send`, `sendTo`, delivery and replay; owned `Arrival`; recordings; the UDP, WebSocket, shared memory, MIDI, serial, gRPC, QUIC and WebRTC transports | `Hub::onDispatch`; `Hub::registerDecoder` for further types; custom feed and network transports; `probeNetworkCache` and `seedNetworkCache`; hub image views with decode options, channels and probes; archives, byte sources, `writeBytes` and `TextCatalog`; `Feed::receivedAt` |
| Texture publication | Nothing in the extension; Sketchbook publishes a Python canvas through its own publisher | `io::publish` publishers, subscriptions and publication listing |

A sketch drains its feeds in `update`; transports never call into Python.

### Libraries without bindings

No Python module exposes these libraries. SigilScry, SigilSubstance and
SigilUSD build only when their optional SDKs are found, and nothing reports to
a Python process whether those SDKs are available.

| Library | Native surface the catalog uses | Sketches |
| --- | --- | --- |
| SigilVideo | `video::decodeVideo`, `Video::frameAt` and `video::Playback`; Compose `video` leaves | `sticker_collection`, `video_compose`, `video_compositing` |
| SigilScry | `scry::WebEngine`, `WebView` and `WebImage`; Compose `web` leaves; `sketch::scry` settling | `import_native`, `web_panel`, `web_script` |
| SigilSubstance | `substance::Package` and `substance::Graph` | `substance_swatches` |
| SigilUSD | `usd::Writer`, `readModel`, `readLights` and `readCameras` | `usd_roundtrip` |
| SigilMeasure | `measure::CheckTable`, `check`, `reading`, `finding`, `heading` and `lineFit` | `black_watch`, `cde_motif`, `chaucer_astrolabe`, `chevreul_circle`, `dunhuang_star_chart`, `eva_magi_defense`, `fallout2_charsheet`, `minard_1869`, `penrose_paving`, `sigillum_aemeth`, `slitscan_2001`, `spacejam_1996`, `thunder_fulu`, `xcom_battlescape` |

## Python studies

The bundled Python sketches are authoring studies. Five port a native sketch,
three adapt one, and the rest exercise the Python surface without a native
counterpart. `python_observable_flowfield` and
`python_observable_reaction_diffusion` need NumPy, which the package's
`studies` extra supplies.

| Study | What it combines | Native sketch |
| --- | --- | --- |
| `python_hello` | A greeting and a moving circle, with two constants to edit | — |
| `python_hello_compose` | Python components, native kits, material paints and an entrance | — |
| `python_orbits` | Periodic arcs and fading trails computed from the scene clock | — |
| `python_dashboard` | Functions and iterables that build retained elements | — |
| `python_type_atelier` | Mixed runs, inline slots, named selections, initial letters, balanced story frames and a curved baseline | — |
| `python_document` | One document under two inherited role stylesheets | Adapts `document_styles` |
| `python_kit_specimen` | Native layout schemes, page furniture and a memo that restores its scoped theme | — |
| `python_motion_signals` | Shared outputs, native bindings and keyframe motion | — |
| `python_memo_station` | Retained model descriptions, memo invalidation and native motion | — |
| `python_compose_stamps` | Compose trees placed by a Draw pen and repeated as a custom brush tip | — |
| `python_data_garden` | Native table sorting and domain scales over one illustrative CSV dataset | — |
| `python_live_signals` | UDP feed arrivals, JSON decoding, drawn traces and a reply to each sender | — |
| `python_mesh_observatory` | Parametric, lathed and regular meshes with a camera and native lighting | — |
| `python_world_study` | One vessel mesh under keyed scene nodes, three dielectric finishes and a moving three-point rig, rendered on the CPU | — |
| `python_liquid_glass` | Runtime shader refraction, child slots, moving uniforms and Bezier filaments | Ports `p5_refractive_metaballs` |
| `python_botanical_study` | Wash, dry mass, hatching and veins over ten leaves on a pressure-drawn branch | Adapts `brush_botanical_study` |
| `python_liquid_layers` | Nibs, wet fibres, pigment blooms and pattern materials | Adapts `p5_liquid_layers` |
| `python_observable_flowfield` | A NumPy angle field drawn as packed line batches | Ports `observable_flowfield_3` |
| `python_observable_l_system` | String rewriting, a turtle stack and a bounds-fitted pen drawing | Ports `observable_l_system_tree` |
| `python_observable_reaction_diffusion` | A NumPy Gray–Scott simulation and bulk pixel transfer | Ports `observable_reaction_diffusion` |
| `python_observable_reynolds` | Stateful flocking, seeded initialization and transforms | Ports `observable_reynolds_steering` |

`python_world_study` takes material parameters from `sigil.material.kit` and
meshes and cameras from `sigil.geometry.mesh`; no World namespace aliases those
origins.

## Catalog audit

Every registered C++ sketch appears once, under its collection. **Needs**
summarizes the native capabilities its source uses beyond basic pen verbs.
**Not yet bound** names each native surface a translation still needs, in
native spelling; a dash means every requirement is bound. The sketch's own
model, arithmetic and data become Python code and are not listed.

### Start & fixtures

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `hello` | Themed kit page; retained cards; keyless clock-reading pen leaf; output bound to a transform; ticker callback; data-driven re-describe | — |
| `shapeworks_lab` | Star generator; path operator chain; bevel normals; environment map; reflective recipes; sequence tile; raster bakes; spline sweeps and projection; point billboards | `SkSurfaces::Raster` standalone raster surfaces; `geometry::shapes` silhouette generators; `geometry::mesh::pop` point-operator chains; `geometry::mesh::Cloud` and `mesh::points`; `geometry::mesh::curve::Spline3` and frames; `geometry::mesh::pop::sweep` and `geometry::sections`; `geometry::path::operations` chain and outline effects; `material::pattern` stock tiles; `material::Texture` and `Material::slot`; `material::EnvironmentMap` and kit environments; `material::bevelNormals` normal maps; `material::kit` reflections (`gold`, `chrome`, `glass`) |
| `stock_materials` | Field recipes; recipe names; pattern tiles; grained and girih kit materials; SDF materials; kit text paints; unit ramps; kit cells | `material::pattern` stock tiles; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::sdf` shapes and styles; `material::Recipe` authoring; `material::kit` text paints (`water`, `starNest`, `clouds`); `material::kit::girih8` and `GirihPalette` |
| `substance_swatches` | Substance archive cook; channel images by usage; kit backdrop and title card; availability probe; panel grid | `available(why)` probes and `sketch::requireCached`; `sketch::kit::titleCard`; `sketch::kit` panel chrome (`backdrop`, `frame`); `substance::Package` and `substance::Graph` rendering |
| `web_panel` | Shared web engine; web view leaf; web image slot painted by pen; deterministic settle; availability probe | `available(why)` probes and `sketch::requireCached`; `sketch::scry` shared engine, settling and sequences; `compose::web` view leaf; `scry::WebEngine` / `WebView` / `WebImage` |

### Draw

#### Draw

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `bristle_current` | Fixed-rate ticker; seeded noise field with detail; xorshift Chance stream; kept canvas; multiply blending; alpha colors | — |
| `p5_hello` | Kept graphics canvas; translucent background trail; basic pen shapes | — |
| `p5_mixed_forms` | Kit stage; radial material fill; inherited font and ink into pen; retained composition guest; unseeded pen noise; kept-canvas trail | — |

#### Draw · Generative

| Sketch | Needs | Not yet bound | Python study |
| --- | --- | --- | --- |
| `p5_attractor_loom` | Kit stage; runtime shader with grain child slot; blended material ground; canvas-mapped stroke paint; batched canvas paths | — |  |
| `p5_flow_field` | Runtime shader with noise child slot; canvas-mapped stroke; shape-fitted glow paint; grid module sizes; seeded noise detail | — |  |
| `p5_fractal_garden` | Runtime shader with grain child slot; blended material ground; canvas-mapped stroke; batched canvas paths and points; shape-fitted glow | — |  |
| `p5_liquid_layers` | Grid-line pattern tiles; blended ground paint; nib and stock brush tools; brush splines and lines; ring arrangement | — | Adaptation: [`python_liquid_layers.py`](../../spell-circle-canvas/src/sketch/sketches/python_liquid_layers.py) |
| `p5_refractive_metaballs` | Runtime shaders; child shader slot; per-frame vector uniforms; canvas-mapped paints; additive blending; Bezier curves | — | Port: [`python_liquid_glass.py`](../../spell-circle-canvas/src/sketch/sketches/python_liquid_glass.py) |

#### Draw · Observable reproductions

| Sketch | Needs | Not yet bound | Python study |
| --- | --- | --- | --- |
| `observable_circle_packing` | Kit stage; seeded pen random stream; circles and colors | — |  |
| `observable_circle_packing_contained` | Seeded pen random stream; circles and colors | — |  |
| `observable_fibonacci` | Pen clock; centre rectangle mode; filled squares | — |  |
| `observable_fibonacci_rectangles` | Pen clock; HSB color mode; transforms; rectangles | — |  |
| `observable_flowfield_1` | Kit stage; pen clock; square caps; line grid | — |  |
| `observable_flowfield_2` | Mix64 Chance stream; pen clock; stroke paint; batched canvas lines | — |  |
| `observable_flowfield_3` | Kit stage; Mix64 Chance stream; pen clock; stroke paint; batched canvas lines | — | Port: [`python_observable_flowfield.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_flowfield.py) |
| `observable_grid` | Pen clock; degree angle mode; centre rectangle mode; transforms | — |  |
| `observable_l_system` | Mix64 Chance ranges; pen clock; lines and circles | — |  |
| `observable_l_system_tree` | Pen clock; transforms; grayscale lines | — | Port: [`python_observable_l_system.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_l_system.py) |
| `observable_noise` | Seeded pen noise; Mix64 Chance stream; pen clock; polyline shapes | — |  |
| `observable_noise_map` | Seeded three-dimensional pen noise; pen clock; grayscale rectangles | — |  |
| `observable_random_walker` | Mix64 Chance bits; pen clock; distance-shaded circles | — |  |
| `observable_reaction_diffusion` | Image from RGBA buffer; scaled image drawing | — | Port: [`python_observable_reaction_diffusion.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_reaction_diffusion.py) |
| `observable_reynolds_steering` | Persistent model; seeded pen random; HSB color mode with alpha range; translucent trails on kept canvas; transforms; closed shapes | — | Port: [`python_observable_reynolds.py`](../../spell-circle-canvas/src/sketch/sketches/python_observable_reynolds.py) |

#### Draw · Procedural

| Sketch | Needs | Not yet bound | Python study |
| --- | --- | --- | --- |
| `bristle_bloom` | Kit stage; seeded random, Gaussian and noise pen streams; screen and additive blending; Bezier curves; transforms | — |  |
| `brush_botanical_study` | Engine state stack; pencil and charcoal tools; pressure splines, lines; polygon wash, bled fill, mass, hatch; engine circles; ellipse arrangement | — | Adaptation: [`python_botanical_study.py`](../../spell-circle-canvas/src/sketch/sketches/python_botanical_study.py) |
| `brush_custom` | Image-tipped brush tools; shape spacing and scatter; grain images; pressure size response; RGBA-built images; document labels placed by pen | — |  |
| `brush_dynamics` | Stylus input samples; dab sampling and deposit; tilt, barrel rotation and speed tool responses; flat pressure profile | — |  |
| `brush_engine_atlas` | Brush engine catalogue; custom pen and image tips; pressure curve; field-warped bled fill; polygon mass and hatch arrays | — |  |
| `brush_live_tutorial` | Brush engine and custom tips; stock-field flow lines; hatched polygons; bled fill rectangles; engine splines; kept canvas; pointer, clock | — |  |
| `brush_rain` | Stock brush tools; pressure profiles; flow lines through a custom direction function; kept graphics node | — |  |
| `brushwork_currents` | Kit stage; stock brush tools; wave and vortex fields; flow lines; pressure splines; brush lines | — |  |

### Kit

#### Kit · API

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `blend_options` | Outline blend runs with keys, spacing, spines and orientation; shape generators; comparison kit; section headers | `sketch::kit::sectionHeader`; `geometry::shapes` silhouette generators; `geometry::shapes` parametric curve generators; `geometry::path::blend` shape blends |
| `blur_falloff` | Image-filter and map-driven blur effects; bound uniform; unit ramps; sequence tile; kit dots; section headers | `sketch::kit::sectionHeader`; `material::pattern` stock tiles; `material::skia::Effect::filter` |
| `border_weave` | Silhouette-following border modes; double and weighted borders; woven braid brush; crossing rule; shape generators; section headers | `sketch::kit::sectionHeader`; `compose::Border` and `compose::decorations::border` rules; `compose::brush::weave` strand composite; `compose::kit::braid` strands; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators; `geometry::path` crossings and crossing rules |
| `bullets_dropcap` | Initial letters; nested opening styles; flow around a shaped ornament; hanging list kit; font fallback; wells | `compose::kit` typesetting (`bullets`, `nestedRun`, `columns`, `rules`); `geometry::shapes` silhouette generators |
| `cascade` | Inherited type, ink transitions, classes and custom properties; lexical environment values; pen and graphics leaves; readout kit | `sketch::kit::readout`; `core::environment::Provide` / `inheritedOr` values |
| `channel_bind` | OSC connection on the hub; kit channels writing outputs; binding chains; recording mount; readout kit | `sketch::kit::readout`; `sketch::kit::Channel` address follower; `data::Connection` |
| `cjk_rules` | Vertical Japanese blocks; stock and house kinsoku; hanging punctuation table; mojikumi bracket spacing; tsume | `weave::kit` kinsoku and hanging stock tables |
| `codec_roundtrip` | PLY encode and decode of meshes and clouds; model merge, bounds and fit; point attributes; billboard splats; mesh painter | `sketch::kit::sectionHeader`; `geometry::mesh::Cloud` and `mesh::points`; `geometry::mesh::codec` |
| `compute_variant` | Set-kind hosting; wave rail; compute pass cooking a point chain; stamped geometry pass; variant repaint; readback callback | Set sketches (`sketch::SetContext`, `describe`); `geometry::mesh::pop` point-operator chains; `geometry::mesh::curve::Spline3` and frames; `world::kit` rails (`wave`, `winding`, `rail`); `world::computePass` passes and `world::readback` |
| `contour_poses` | Measured contours; poses by distance with wrap policies; corner detection and windows; shape generator; pen leaves | `sketch::kit::sectionHeader`; `geometry::shapes` silhouette generators; `geometry::path::Contour` corners, poses and offsets |
| `corner_notched` | Corner treatments as comparable shapes: rounding wrapper, chamfers and notches with corner masks; comparison kit | `sketch::kit::sectionHeader`; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators |
| `coverage_boundary` | Coverage boundary; outer glow and inner shadow layer styles; alpha cut-out images; shape generators; section headers | `sketch::kit::sectionHeader`; `compose::styles` layer styles (glow, inner shadow, bevel); `SkSurfaces::Raster` standalone raster surfaces; `geometry::shapes` silhouette generators |
| `crossing_rule` | Crossing discovery; crossing rules (alternate, sequence, pairs, except); crossing patches; canvas clipping; comparison kit | `sketch::kit::sectionHeader`; `geometry::path` crossings and crossing rules |
| `crt_bloom` | Glow and directional blur effects; additive blend; texture cache; CRT overlay recipe; explicit default face | `sketch::kit::sectionHeader`; `weave::defaultFace` in a `weave::Type` partial |
| `decay_step` | Clock arithmetic (decay, quantize, step, phase); closed-form spring state; plot kit with axes, rules and traces | `sketch::kit::plot` chart layers; `motion::Spring` value and `motion::spring` step |
| `ember_decode` | Kinetic text track with pass effect and stagger by unit; beat schedule query; authored SkSL recipe; meter kit | `sketch::kit::meter` and `gauge`; `compose::Element::fx` text tracks; `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::Composer::beatsOf` and `units`; `motion::Spread` and `motion::Cascade` schedules; `material::Recipe` authoring |
| `encode_write` | Image encode and decode across formats; hub mount, write and image load; offscreen pen drawing; wells | `sketch::kit::sectionHeader`; `SkSurfaces::Raster` standalone raster surfaces |
| `env_faces` | Environment maps from faces, cube sheets and panoramas; ground replacement; bevel normals; chrome reflection fill | `sketch::kit::sectionHeader`; `SkSurfaces::Raster` standalone raster surfaces; `material::Texture` and `Material::slot`; `material::EnvironmentMap` and kit environments; `material::bevelNormals` normal maps; `material::kit` reflections (`gold`, `chrome`, `glass`) |
| `env_lanes` | World environment node dials; studio and sunset panoramas; world frames baked to images; PBR surfaces | `sketch::kit::sectionHeader`; `material::EnvironmentMap` and kit environments; `world::Environment` and `Element::environmentMap` |
| `exact_tangent` | Text on paths with exact tangents; spiral and oval generators; element rasterized to pixels; region crops; border decoration | `sketch::kit::sectionHeader`; `compose::TextureScene` and `SketchContext::textureScene`; `compose::Border` and `compose::decorations::border` rules; `geometry::shapes` silhouette generators; `geometry::shapes` parametric curve generators |
| `exr_channels` | Float EXR encode; metadata probe; named channel planes; material texture slot readback; availability probe | `available(why)` probes and `sketch::requireCached`; `sketch::kit::sectionHeader`; `image::encodeImage` float pixmaps (`Format::Exr`); `image::probeImage` and `image::decodeChannels`; `material::Texture` and `Material::slot` |
| `feed_vitals` | Hub mount and feed; arrival queue, latest bytes and feed vitals; section header kit; pen strip | `sketch::kit::sectionHeader` |
| `floating_panels` | Compose texture bakes; gauge kit; image panels; textured cylinder and grid meshes; process painter runtime | `sketch::painterRuntime` mesh executor query; `sketch::kit::meter` and `gauge`; `compose::TextureScene` and `SketchContext::textureScene`; `geometry::mesh::render::Runtime` selection |
| `formation_bands` | Polygon silhouette; width profiles, rail offsets and band regions; wave shaper; comparison kit; canvas path strokes | `sketch::kit::sectionHeader`; `geometry::shapes` silhouette generators; `geometry::shapers` and `path::Shaper`; `geometry::path::Profile` width profiles and bands |
| `frame_grid` | Polar frames with angle conventions; unit grid with snap; arrange rings and cells; pen graphics; comparison kit | `sketch::kit::sectionHeader`; `geometry::path::Frame` and `path::Grid` |
| `frame_inputs` | Recipe with frame inputs; uniform block binding; material specialization; material fill with content scale and world transform | `sketch::kit::sectionHeader`; `material::Recipe` authoring; `material::skia::fill` with `FrameData` |
| `fx_scatter_mix` | Per-glyph effect tracks; scatter, mix and tint effects; spread schedules; beat readback meters; comparison kit | `compose::Element::fx` text tracks; `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::Composer::beatsOf` and `units`; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `compose::kit::trackMeter` and `restGhost` instruments; `motion::Spread` and `motion::Cascade` schedules |
| `geo_groups` | Point clouds with named lanes; Houdini geo encode and decode; masked point operators; billboard sprites; comparison kit | `sketch::kit::sectionHeader`; `compose::kit::dotSprite` point stamp; `geometry::mesh::pop` point-operator chains; `geometry::mesh::Cloud` and `mesh::points`; `geometry::mesh::codec` |
| `gif_frames` | Network image load; decoded frames and timed playback; resource and image probes; nearest sampling; cached-resource availability gate; comparison kit | `available(why)` probes and `sketch::requireCached`; `sketch::kit::sectionHeader`; `image::ImageAsset` frames, repetition and wrap; `io::Hub` typed probes and `registerDecoder` for further types |
| `grid_layouts` | Grid, diagonal and baseline layout schemes; ladder rules; themed mono type; readout rows | `sketch::kit::readout` |
| `half_float` | Float raster surface; half-float and byte pixel readback; image from pixels; comparison kit | `sketch::kit::sectionHeader`; `SkSurfaces::Raster` standalone raster surfaces; `skia::isFloatImage` / `halfFloatPixels` float readback |
| `hit_slots` | Slots; hit test, bounds and route queries; keyed connectors; star and blob silhouettes; readout rows | `sketch::kit::readout`; `compose::connector`, `rail` and routers; `compose::routers` stock routers; `geometry::shapes` silhouette generators |
| `hub_reload` | Standalone hub mount, text, image and poll; custom decoder; offscreen pen to PNG; readout rows | `sketch::kit::readout`; `SkSurfaces::Raster` standalone raster surfaces; `io::Hub` typed probes and `registerDecoder` for further types |
| `import_native` | Set hosting; compose texture scene; material texture slots; Scry web view frames; world elements, lights, quads | Set sketches (`sketch::SetContext`, `describe`); `available(why)` probes and `sketch::requireCached`; `sketch::scry` shared engine, settling and sequences; `compose::TextureScene` and `SketchContext::textureScene`; `material::Texture` and `Material::slot`; `scry::WebEngine` / `WebView` / `WebImage` |
| `keeps_and_frames` | Rich stories; paragraph keeps; threaded frames; first-baseline and distribution placement; comparison kit | — |
| `lane_retarget` | Standalone ticker; animated float lanes; slot and family retargeting; chart kit plots; readout rows | `sketch::kit::readout`; `sketch::kit::plot` chart layers; `motion::Ticker::timeline` and standalone tickers; `motion::Lane` retargeting over `AnimatedFloats` |
| `live_settling` | Offscreen composer frame driving; live Knuth-Plass passages; settling reports; comparison kit; readout rows | `sketch::kit::readout`; Standalone `compose::Composer` over a caller canvas; `SkSurfaces::Raster` standalone raster surfaces |
| `material_atlas` | Produced sheet texture; atlas grid, TexturePacker and Aseprite imports; wrapping sequences; texture shader draws | `sketch::kit::sectionHeader`; `SkSurfaces::Raster` standalone raster surfaces; `material::Texture` and `Material::slot`; `material::Atlas` sprite sheets |
| `material_slots` | Runtime shader child slots and uniforms; nearest image paints; LUT images; material kits; masked material stacks | `sketch::kit::sectionHeader`; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::over` layering and masks; `material::skia::Paint::image` sampling options |
| `matte_luma` | Track matte gates; checker and run patterns; ramp-baked coverage image; unit gradients; comparison kit | `sketch::kit::sectionHeader`; `compose::Element::mask` and `compose::by` gates; `SkSurfaces::Raster` standalone raster surfaces; `material::pattern` stock tiles; `material::skia` ramp shaders |
| `mesh_generators` | Extrude, revolve, torus, superellipsoid; spline sweep; spline points with frame lanes; instanced quads; lit mesh draws | `geometry::shapes` silhouette generators; `geometry::mesh::Cloud` and `mesh::points`; `geometry::mesh::curve::Spline3` and frames; `geometry::mesh::pop::sweep` and `geometry::sections` |
| `mesh_normal_bridge` | Offscreen normal and UV mesh passes; environment reflection recipes; bevel normals; shader coverage compositing; squircle silhouette | `SkCanvas` layers, `drawPaint` and `drawImageRect`; `geometry::shapes` silhouette generators; `material::Texture` and `Material::slot`; `material::EnvironmentMap` and kit environments; `material::bevelNormals` normal maps; `material::kit` reflections (`gold`, `chrome`, `glass`) |
| `net_policy` | Seeded network cache; per-hub network policies; image load; PNG encode; specimen wells | `sketch::kit::sectionHeader`; `SkSurfaces::Raster` standalone raster surfaces; `io::seedNetworkCache` network cache seeding |
| `nine slice` | Generated ornament textures; nine-slice decoration with density; direct lattice draw; per-frame redescribe | `sketch::kit::sectionHeader`; `compose::Slice` nine-slice decoration; `compose::kit::ornament` palettes, frames and marks; `skia::draw::drawLattice` nine-slice drawing; `image::ImageAsset` frames, repetition and wrap |
| `ocio_view` | OCIO transforms baked to LUT materials; produced wedge texture; content slot; material-grounded wells | `sketch::kit::sectionHeader`; `SkSurfaces::Raster` standalone raster surfaces; `material::Texture` and `Material::slot`; `material::ocio` colour management |
| `optical_kerning` | Optical kerning shaping; context measurement; cells, captions and readout rows | `sketch::kit::readout` |
| `over_under` | Material stacks and blends; constant, map, height and slope masks; material kits; produced texture; shape corners | `sketch::kit::sectionHeader`; `SkSurfaces::Raster` standalone raster surfaces; `geometry::shapes` corner operators; `material::Texture` and `Material::slot`; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::bevelNormals` normal maps; `material::over` layering and masks |
| `painter_gpu` | Mesh painter with selectable runtime; perspective image panels; element trees baked to textures; specimen furniture | `sketch::painterRuntime` mesh executor query; `sketch::kit::sectionHeader`; `compose::TextureScene` and `SketchContext::textureScene`; `geometry::mesh::render::Runtime` selection |
| `path_booleans` | Silhouette generators; path booleans, offsets, distorts and operation chains; pen shapes; document headings | `geometry::shapes` silhouette generators; `geometry::path::operations` chain and outline effects |
| `pattern_sequence` | Baked pattern tiles: sequence program, sampling remaps and filter, shared-bake copies; tile paints as well grounds | `material::pattern` stock tiles; `material::pattern::Tile` programs |
| `pixfont_dotsprite` | Aliased glyph mask bakes; pixel font blits; dot sprite; tinted image stamps; live pen readout | `sketch::kit::sectionHeader`; `compose::kit` pixel type (`bakeRun`, `Mask`, `PixFont`); `compose::kit::dotSprite` point stamp |
| `place_repeat_tiles` | Instance atlas and pools with repeat placer; sliceable picture tiles and windows; star silhouette; gradient fill | `sketch::kit::sectionHeader`; `compose::instancing` atlases, pools and instances; `compose::tiles` sliceable pictures and windows; `compose::instancing::place` placers; `geometry::shapes` silhouette generators |
| `pop_billboards` | Point operator chains on meshes and polylines; noise and relax; atlas lane; billboard splat sink; sprite bake | `SkSurfaces::Raster` standalone raster surfaces; `geometry::mesh::pop` point-operator chains; `geometry::mesh::Cloud` and `mesh::points` |
| `pop_deform` | Point chains: feathered region select, masked twist, taper, bend, orient and peak; lane-driven billboard sink; dot sprite | `compose::kit::dotSprite` point stamp; `geometry::mesh::pop` point-operator chains; `geometry::mesh::Cloud` and `mesh::points` |
| `pop_math` | Point chains: Math, Affine, lane lookup, Select, masked, keep/drop, fill/mix, normal/peak; billboard sink | `geometry::mesh::pop` point-operator chains; `geometry::mesh::Cloud` and `mesh::points` |
| `pop_order` | Point chain with depth-lane ramp and order permutation; unsorted billboard sink; sprite bake; readout | `sketch::kit::readout`; `SkSurfaces::Raster` standalone raster surfaces; `geometry::mesh::pop` point-operator chains; `geometry::mesh::Cloud` and `mesh::points` |
| `pop_prims` | Mesh primitive lanes and vertex bake; primitive-colour mesh style; promoted point Id stamps; OKLab ramp; readouts | `sketch::kit::readout`; `geometry::mesh::pop` point-operator chains; `geometry::mesh::Mesh` primitive lanes |
| `pop_stamps` | Silhouette motifs baked to atlas; point chains swept into tubes, stamped quads, profile sweeps, windowed ribbon; mesh painter | `compose::TextureScene` and `SketchContext::textureScene`; `geometry::shapes` silhouette generators; `geometry::mesh::pop` point-operator chains; `geometry::mesh::pop::sweep` and `geometry::sections` |
| `rich_slot_reserve` | Rich text inline slots with keyed children; reserved line bands; measured blocks; house face; specimen furniture | — |
| `routers_straight` | Keyed connectors with stock routers (straight, orthogonal bends, arc); anchor rail with octilinear router; path formats | `sketch::kit::sectionHeader`; `compose::connector`, `rail` and routers; `compose::routers` stock routers |
| `routes_probe` | Connectors with routers; standalone profiling composer; route index; cache verdict profile rows; pen leaf; readout classes | `sketch::kit::sectionHeader`; `compose::connector`, `rail` and routers; Standalone `compose::Composer` over a caller canvas; `compose::Composer::profile` node costs; `compose::routers` stock routers; `SkSurfaces::Raster` standalone raster surfaces |
| `sdf_star` | Signed-distance star shapes; layered SDF style (shadow, glow, fill, border); pad reserve; material paint fill | `sketch::kit::sectionHeader`; `material::sdf` shapes and styles |
| `slang_portable` | Runtime Slang module compile; reflected uniform layout and writer; material recipe Slang sources; diagnostics readouts | `sketch::kit::sectionHeader`; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::Recipe` authoring; `material::slang` module compiler |
| `spacing_passes` | Justified Knuth-Plass paragraphs with word, letter and glyph-scale passes; single-word justification; captioned wells; cells | — |
| `surface_components` | Compose kit sheet, panel grid and wells; material and live fill grounds; gel layer style; update hook | `compose::kit::aquaGel`, `y2kChrome`, `gloss` finishes |
| `svg_silhouette` | SVG path-data silhouette with stretch or aspect fit; stroked keylined boxes; comparisons | `sketch::kit::sectionHeader`; `geometry::shapes` silhouette generators |
| `ticker_lanes` | Standalone manually stepped ticker; steppables, fixed rate, derivation, timeline ramp; recorded-lane plots | `sketch::kit::plot` chart layers; `motion::Ticker::timeline` and standalone tickers |
| `tile map` | Memoized chunks over one shared image asset with regions; bound flash opacity; profiling probe composer; readouts | `sketch::kit::sectionHeader`; `sketch::kit::readout`; Standalone `compose::Composer` over a caller canvas; `compose::image` shared `image::ImageAsset` leaf |
| `usd_roundtrip` | USD stage writer and readers for mesh, instancer, light, camera; point scatter on mesh; mesh painter | `available(why)` probes and `sketch::requireCached`; `sketch::kit::sectionHeader`; `geometry::mesh::Cloud` and `mesh::points`; `geometry::mesh::codec`; `usd::Writer` stage export; `usd::readModel` / `readLights` / `readCameras` import |
| `volatility_cost` | Eager profiling probe composer: stats, profile, cache states, refusals, bounds; star silhouettes; bound movers; legend, table, readout | `sketch::kit::readout`; `sketch::kit::table`; `sketch::kit::legend` and `swatchStrip`; Standalone `compose::Composer` over a caller canvas; `compose::Composer::profile` node costs; `geometry::shapes` silhouette generators |
| `warichu_placeholder` | Warichu split of a note paragraph; vertical writing; rich text slots; measured advances; readout | `sketch::kit::readout` |
| `web_script` | Shared web engine views; HTML load, script, scroll and press sequences settled to page state; frame stills | `available(why)` probes and `sketch::requireCached`; `sketch::scry` shared engine, settling and sequences; `sketch::kit::sectionHeader`; `scry::WebEngine` / `WebView` / `WebImage` |
| `yarn_marquee` | Winding spline; parallel-transport and hung rail frames; frame sweep with line section; textured two-sided mesh painter | `SkSurfaces::Raster` standalone raster surfaces; `geometry::mesh::curve::Spline3` and frames; `geometry::mesh::pop::sweep` and `geometry::sections`; `world::kit` rails (`wave`, `winding`, `rail`) |

#### Kit · Depth

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `card_flip` | Retained 3D transforms, preserve3d and hidden backfaces; bound rotation lanes; document kit; specimen theme and stage | — |

### Compose · Typography

| Sketch | Needs | Not yet bound | Python study |
| --- | --- | --- | --- |
| `document_styles` | Document kit roles; role stylesheets with block leading; inline rich run; specimen page, comparison and wells | — | Adaptation: [`python_document.py`](../../spell-circle-canvas/src/sketch/sketches/python_document.py) |

### Specimen

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `bound_lane` | Bound-lane chain stages and wiggle; motion paths with travel; specimen chart plots and readout; stock silhouettes | `sketch::kit::readout`; `sketch::kit::plot` chart layers; `geometry::shapes` silhouette generators |
| `curve_shelf` | Stock parametric curve silhouettes; shaped stroked boxes; specimen cells, comparisons and section headers | `sketch::kit::sectionHeader`; `geometry::shapes` parametric curve generators |
| `field_shelf` | Shader field recipes; slotted child texture; custom canvas drawing; specimen comparisons and section headers | `sketch::kit::sectionHeader` |
| `gerstner grid` | Anisotropic grid-line tiles; noise field paint; staggered entrance transitions; bound sweep output; re-describe on update; grid arithmetic | `material::pattern` stock tiles |
| `kinetic_card` | Per-glyph text effect tracks and presets on a cascade; shared phase output; beat meters read from composer; specimen theme | `compose::Element::fx` text tracks; `compose::Composer::beatsOf` and `units`; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `compose::kit::trackMeter` and `restGhost` instruments; `motion::Spread` and `motion::Cascade` schedules |
| `noise_shelf` | Counter, state and lattice mixers; cache key folds; pen program fields; specimen cells, wells and section headers | `sketch::kit::sectionHeader`; `core::noise::hash` / `lattice` positional hashes; `core::hash` FNV-1a words |
| `paint_shelf` | Radial, conical, sweep and unit gradients; world-space paint; revisioned pixel buffer paint; specimen section headers | `sketch::kit::sectionHeader`; `material::skia::PixelBuffer` and `Paint::buffer` |
| `paragraph_paints` | Text fill recipes; chrome ramps; shader local matrix; justified Knuth-Plass text with pattern hyphenation; measured passage file | `compose::kit::sunsetChromeType`, `silverChromeType` paints; `weave::HyphenationOptions::patterns` on a Block; `SkShader` local matrices and raw image shaders; `material::kit` text paints (`water`, `starNest`, `clouds`); `material::skia::Paint` SkShader interop |
| `paragraph_sheet` | Paragraph styles: leading kinds, spacing, indents, justification, tab stops, vertical writing; caption kit; panel grids, ladders | `weave::HyphenationOptions::patterns` on a Block |
| `shape_tour` | Stock silhouette generators, parametric curves and corner operators; shaped filled boxes; captions and cell runs | `geometry::shapes` silhouette generators; `geometry::shapes` corner operators; `geometry::shapes` parametric curve generators |
| `stroke_atlas` | Line, rail and layered brush decorations; shapers; stamp and corner patterns; borders and hatches; stock silhouettes; animated dashes | `compose::keyedShape` comparable shapes; `compose::lines::Line` and `lines::Rails`; `compose::onEdges` and `compose::inset` adaptors; `compose::Border` and `compose::decorations::border` rules; `compose::Brush` shaped layer stacks; `compose::brush` scatter, pattern and art brushes; `compose::lines::Hatch` and `lines::RadialHatch`; `compose::Wash` material wash; `compose::ContourWalk` path-walk stamps; `compose::lines::presets` and `compose::brush::presets`; `SkPathBuilder::arcTo` and `skpathutils::FillPathWithPaint`; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators; `geometry::shapes` parametric curve generators; `geometry::shapers` and `path::Shaper` |
| `text_paints` | Text fill recipes mapped to run metrics; chrome ramps; shader local matrix; wells and comparisons | `compose::kit::sunsetChromeType`, `silverChromeType` paints; `SkShader` local matrices and raw image shaders; `material::kit` text paints (`water`, `starNest`, `clouds`); `material::skia::Paint` SkShader interop |
| `ui particles` | Instanced atlas stamping from a live pool; fixed-step simulation; ornament nine-slice and flourish cards; scrim; gradients | `compose::instancing` atlases, pools and instances; `compose::Slice` nine-slice decoration; `compose::kit::ornament` palettes, frames and marks; `compose::kit::flourish` vines and cards; `compose::kit::scrim` and `drawHaloed` legibility |

### Set

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `dart_flight` | Set hosting; kit winding spline; swept rail; along-curve placement; revolved and torus meshes; PBR surfaces; lit set kit | Set sketches (`sketch::SetContext`, `describe`); `geometry::mesh::curve::Spline3` and frames; `geometry::mesh::pop::sweep` and `geometry::sections`; `world::kit` rails (`wave`, `winding`, `rail`); `world::Element::along`, `staggerChildren` and `node` |
| `deformed_cloud` | Set hosting; surface point scatter; masked point-operator chain; stamped facets; PBR surface; lit set kit | Set sketches (`sketch::SetContext`, `describe`); `geometry::mesh::pop` point-operator chains; `geometry::mesh::Cloud` and `mesh::points`; `world::Element::chain`, `stamp` and `window` |
| `first_light` | Set hosting; kit wave and rail curves; pose along; swept tube; windowed stamp chain; sun and point lights; turntable | Set sketches (`sketch::SetContext`, `describe`); `geometry::mesh::pop` point-operator chains; `geometry::mesh::curve::Spline3` and frames; `geometry::mesh::pop::sweep` and `geometry::sections`; `world::kit` rails (`wave`, `winding`, `rail`); `world::Element::chain`, `stamp` and `window` |
| `glow_trail` | Set hosting with camera; kit wave curve; windowed stamp chain; tagged geometry and post passes with previous-frame feedback | Set sketches (`sketch::SetContext`, `describe`); `geometry::mesh::pop` point-operator chains; `geometry::mesh::curve::Spline3` and frames; `world::kit` rails (`wave`, `winding`, `rail`); `world::Element::chain`, `stamp` and `window` |
| `key_light` | Set hosting; preset tree inspection; emitter intensity and emission bound to outputs; three-point rig; lit set | Set sketches (`sketch::SetContext`, `describe`); `world::Element::along`, `staggerChildren` and `node` |
| `lantern_room` | Set hosting; sun, spot and point emitters; unlit emissive and PBR surfaces; superellipsoids; turntable camera | Set sketches (`sketch::SetContext`, `describe`) |
| `material_lab` | Set hosting on the device tier; texture-slotted PBR surfaces; masked material stack; usage-keyed texture set; generated maps | Set sketches (`sketch::SetContext`, `describe`); `material::Texture` and `Material::slot`; `material::over` layering and masks; `material::kit::surface` over `texture::TextureMaps` |
| `reflection_lab` | Set hosting; environment maps with crossfade and backdrop; chrome, metal, dielectric and glass surface presets | Set sketches (`sketch::SetContext`, `describe`); `material::EnvironmentMap` and kit environments; `world::Environment` and `Element::environmentMap` |
| `scattered_model` | Set hosting; resource hub blob; glTF mesh decode and fit; surface scatter chain; stamped facets; unlit core | Set sketches (`sketch::SetContext`, `describe`); `geometry::mesh::pop` point-operator chains; `geometry::mesh::codec`; `world::Element::chain`, `stamp` and `window` |
| `scene_surfaces` | Set hosting; compose scenes rendered to textures; base-colour texture slots with tiling; swept ribbon; unlit screens | Set sketches (`sketch::SetContext`, `describe`); `compose::TextureScene` and `SketchContext::textureScene`; `geometry::mesh::curve::Spline3` and frames; `geometry::mesh::pop::sweep` and `geometry::sections`; `material::Texture` and `Material::slot`; `world::kit` rails (`wave`, `winding`, `rail`) |
| `set_stagger` | Set hosting with camera; world child stagger by spread; entrance transitions; selector-narrowed passes; three-point rig | Set sketches (`sketch::SetContext`, `describe`); `motion::Spread` and `motion::Cascade` schedules; `world::Element::along`, `staggerChildren` and `node` |

### Data

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `artnet_lights` | Art-Net Connection handlers and sends; recording mount; eased ticker ramps; instrument page; themed pen drawing | `sketch::kit::instrument`; `motion::Ticker::timeline` and standalone tickers; `data::Connection` |
| `data_scales` | Scale transforms; chart plot kit layers; stylesheet classes; stated default face; caption and panel grid | `sketch::kit::plot` chart layers; `weave::defaultFace` in a `weave::Type` partial |
| `data_sources` | Asset table and database; DuckDB memory query; bars and section header kit; comparison well | `sketch::kit::sectionHeader`; `sketch::kit::bars` over `data::Table` |
| `feed_events` | JSON Connection kind handlers; recording mount; eased ticker ramps; instrument page; pen bands | `sketch::kit::instrument`; `motion::Ticker::timeline` and standalone tickers; `data::Connection` |
| `feed_sky` | Schema-backed Connection; per-sketch FlatBuffers schema; recording mount; instrument page; pen bands | Per-sketch `<stem>.fbs` schemas for Python entries; `sketch::kit::instrument`; `data::Connection`; `data::FlatBuffer` roots and `data::schema` |
| `grpc_watch` | gRPC Connection handlers and broadcast sends; eased ticker ramps; instrument page; pen bands | `sketch::kit::instrument`; `motion::Ticker::timeline` and standalone tickers; `data::Connection` |
| `midi_pads` | MIDI Connection in and out; eased ticker ramps; instrument page; pen cells and knob | `sketch::kit::instrument`; `motion::Ticker::timeline` and standalone tickers; `data::Connection` |
| `osc_desk` | OSC Connection address handlers and replies; chained ticker ramps; instrument page; pen faders | `sketch::kit::instrument`; `motion::Ticker::timeline` and standalone tickers; `data::Connection` |
| `phone_sky` | WebSocket Connection with served pages; broadcast sends; eased ticker ramps; instrument page | `sketch::kit::instrument`; `motion::Ticker::timeline` and standalone tickers; `data::Connection` |
| `quic_sky` | QUIC Connection and its feed arrivals; recording mount; instrument page; pen bands | `sketch::kit::instrument`; `data::Connection` |
| `schema_scene` | Generated schema values; FlatBuffer hub decoder; schema-backed Connection; instrument page; document labels | Per-sketch `<stem>.fbs` schemas for Python entries; `sketch::kit::instrument`; `data::Connection`; `data::FlatBuffer` roots and `data::schema` |
| `serial_sensor` | Serial line Connection; recording mount; instrument page; transformed pen ribbons | `sketch::kit::instrument`; `data::Connection` |
| `shared_sky` | Shared-memory Connection; recording mount; instrument page; pen bands | `sketch::kit::instrument`; `data::Connection` |
| `webrtc_sky` | WebRTC Connection over WebSocket signal; broadcast sends; eased ticker ramps; instrument page | `sketch::kit::instrument`; `motion::Ticker::timeline` and standalone tickers; `data::Connection` |

### Media

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `guest_body` | Set-kind world frame; published texture subscription; texture scene; material texture slot; lit meshes | Set sketches (`sketch::SetContext`, `describe`); `sketch::Guest` publication subscriptions; `compose::TextureScene` and `SketchContext::textureScene`; `material::Texture` and `Material::slot` |
| `guest_picture` | Published frame subscription on canvas recorder; specimen page, caption and well; document waiting card | `sketch::Guest` publication subscriptions |
| `sticker_collection` | Animated image assets; streamed video frames; cached-resource gate; pen image transforms; document header | `available(why)` probes and `sketch::requireCached`; `video::decodeVideo` and `video::Video::frameAt` |
| `video_compose` | Compose video leaves; streaming decoders; playback scheduler; module grid arrangement; ticker-driven cover | `available(why)` probes and `sketch::requireCached`; `compose::video` leaf and `compose::VideoOptions`; `video::decodeVideo` and `video::Video::frameAt`; `video::Playback` scheduled frame requests |
| `video_compositing` | Streaming video decoders; playback scheduler; raw canvas image compositing with blend and alpha | `available(why)` probes and `sketch::requireCached`; `SkCanvas` layers, `drawPaint` and `drawImageRect`; `video::decodeVideo` and `video::Video::frameAt`; `video::Playback` scheduled frame requests |

### Catalog

#### Catalog · Type

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `annotated_margin` | Document paragraphs; per-word cascade track; composer unit and beat read-backs; sibling annotations; block rules; track meter | `compose::Element::fx` text tracks; `compose::Composer::beatsOf` and `units`; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `compose::kit::trackMeter` and `restGhost` instruments; `compose::kit` typesetting (`bullets`, `nestedRun`, `columns`, `rules`); `compose::kit::annotate` sibling annotations; `motion::Spread` and `motion::Cascade` schedules |
| `beethoven` | Span-revealed arc strokes; entrance animation; stylesheet classes; drop shadow; keyed absolute layout | `compose::Element::mask` and `compose::by` gates; `geometry::shapes` silhouette generators |
| `bousen` | Vertical blocks; decorated span paints; OpenType features; phrase marks; column cascade track; kit cells | `compose::Element::fx` text tracks; `compose::Element::mark` selection marks; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules |
| `chrome_type` | Layer style presets on glyph boundaries; glow and bevel decorations; themed kit cells and page | `compose::styles` layer styles (glow, inner shadow, bevel); `compose::kit::aquaGel`, `y2kChrome`, `gloss` finishes |
| `horizontal_flow` | Document kit; specimen theme, wells and captions; stock silhouettes; shape-following flowAround exclusions | `geometry::shapes` silhouette generators; `geometry::shapes` corner operators |
| `manuscript` | Ornament kit; nested small-caps run; Knuth-Plass with pattern hyphenation; flowAround exclusions; texture cache; page-turn update | `compose::kit::ornament` palettes, frames and marks; `weave::HyphenationOptions::patterns` on a Block |
| `mawarikomi` | Vertical writing mode; silhouette exclusions; clamped column ellipsis; document kit; specimen cells; gradient ground | `geometry::shapes` silhouette generators |
| `ruby_kenten` | Vertical writing mode; reserved ruby and kenten annotations; specimen cells with style classes; gradient ground | — |
| `tategaki` | Vertical rich text forms; span paint; per-cluster kinetic entrance; document kit; specimen cells | `compose::Element::fx` text tracks; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules |
| `threaded_story` | Threaded story frames; kit columns; exclusion flow; paragraph styles; specimen cells and wells | `compose::kit` typesetting (`bullets`, `nestedRun`, `columns`, `rules`) |

#### Catalog · Chrome

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `aero desktop` | Runtime shaders; blended paint stacks; constant Gaussian layer blur; text glow; delayed opacity ramp; bound opacity outputs; entrance transitions; texture cache | — |
| `y2k chrome` | Gel and chrome layer presets; bevel emboss; per-edge strokes; stock silhouettes; marquee ticker; text paint layers; pattern tiles | `sketch::kit::ticker` marquee strip; `compose::styles` layer styles (glow, inner shadow, bevel); `compose::onEdges` and `compose::inset` adaptors; `compose::kit::aquaGel`, `y2kChrome`, `gloss` finishes; `compose::kit::sunsetChromeType`, `silverChromeType` paints; `geometry::shapes` silhouette generators |

#### Catalog · Game UI

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `daemon console` | Structured feed ring; per-glyph text effects; SDF panel; bound tile pan; OCIO output view; meters; bound outputs; rich row classes | `sketch::kit::meter` and `gauge`; `compose::Element::fx` text tracks; `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::feed` rings and text feeds; `compose::Composer::setView` output view; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules; `material::sdf` shapes and styles; `material::skia::Paint` motion-driven uniforms and offsets; `material::ocio` colour management |
| `loot grid` | Instanced atlas stamps; bevel emboss panel; recessed wells; noise recipes; pattern tiles; path silhouettes; legends; bound outputs | `sketch::kit::legend` and `swatchStrip`; `compose::instancing` atlases, pools and instances; `compose::styles` layer styles (glow, inner shadow, bevel); `compose::instancing::place` placers |
| `passive tree` | SDF sockets with bound glow uniform; rails with orbit router; layered brush presets; stock silhouettes; span trims; legend; title card | `sketch::kit::titleCard`; `sketch::kit::legend` and `swatchStrip`; `compose::connector`, `rail` and routers; `compose::lines::presets` and `compose::brush::presets`; `compose::routers` stock routers; `geometry::shapes` silhouette generators; `material::sdf` shapes and styles; `material::skia::Paint` motion-driven uniforms and offsets |
| `persona menu` | Runtime shader on a 6 Hz stepped clock; stock silhouettes; fixed wedge path; spring cursor; staggered entrances; text outlines and glow | `compose::keyedShape` comparable shapes; `motion::Spring` value and `motion::spring` step; `geometry::shapes` silhouette generators |
| `world hud` | Set-kind world frame; HUD texture scene on camera-fitted quad; voxel mesh; kit wells; instanced stamps; tick rail; silhouettes | Set sketches (`sketch::SetContext`, `describe`); `compose::instancing` atlases, pools and instances; `compose::TextureScene` and `SketchContext::textureScene`; `compose::styles` rails, scanlines and stipple; `geometry::shapes` silhouette generators; `geometry::mesh::camera::Camera::extentAt`; `material::Texture` and `Material::slot` |

#### Catalog · Generative

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `flourish` | Ornament and flourish kits; contour walks; nine-slice; paint programs; routed connectors; layer blur; runtime shaders; silhouettes; pen overlays | `compose::onEdges` and `compose::inset` adaptors; `compose::connector`, `rail` and routers; `compose::PaintProgram` canvas decorations; `compose::Slice` nine-slice decoration; `compose::ContourWalk` path-walk stamps; `compose::routers` stock routers; `compose::kit::ornament` palettes, frames and marks; `compose::kit::flourish` vines and cards; `SkSurfaces::Raster` standalone raster surfaces; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators |
| `night network` | Compose brushes and lines; stroke presets; path shapers; routed rails; span masks; SDF recipes; bound paint uniforms; art warp | `compose::keyedShape` comparable shapes; `compose::Element::mask` and `compose::by` gates; `compose::lines::Line` and `lines::Rails`; `compose::connector`, `rail` and routers; `compose::Brush` shaped layer stacks; `compose::brush` scatter, pattern and art brushes; `compose::lines::presets` and `compose::brush::presets`; `compose::routers` stock routers; `geometry::shapes` silhouette generators; `geometry::shapers` and `path::Shaper`; `material::sdf` shapes and styles; `material::skia::Paint` motion-driven uniforms and offsets |

#### Catalog · Tiling

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `zellige` | Girih pattern generator; speckle tile; regenerated pattern fills on update; inner-glow decoration; document type | `compose::styles` layer styles (glow, inner shadow, bevel); `material::pattern` stock tiles; `material::kit::girih8` and `GirihPalette` |

### Study

#### Study · Type

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `axis_ripple` | Per-glyph variable-axis track; shaped pen positions; selector-anchored marks; face axis query; pen meter; document type | `compose::Element::fx` text tracks; `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::metrics` and `compose::runPens` measurement; `compose::Element::mark` selection marks; `motion::Spread` and `motion::Cascade` schedules; `SkTypeface` variation axes and style readback |
| `elastic_type` | Per-glyph keyframe tracks; rest ghost; sketch-kit plots with traces and marks; class stylesheet; document type | `sketch::kit::plot` chart layers; `compose::Element::fx` text tracks; `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::kit::trackMeter` and `restGhost` instruments; `motion::Spread` and `motion::Cascade` schedules |
| `karaoke_wipe` | Per-glyph tint cascade with cue table; schedule read-back; underlaid aliased type; kit marks; bound transforms | `compose::Element::fx` text tracks; `compose::Composer::beatsOf` and `units`; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules |
| `matrix_rain` | Looping per-glyph streak, scramble and mirror tracks; seeded spread; vertical upright text; glow underlays; measured metrics; track meter | `compose::Element::fx` text tracks; `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::kit::trackMeter` and `restGhost` instruments; `motion::Spread` and `motion::Cascade` schedules |
| `rota_convocationis` | Curved-baseline glyph tracks and pass; spread schedules; beat read-back; selector marks; SDF and SkSL recipes; path roughen/resample; instanced embers | `compose::Element::fx` text tracks; `compose::instancing` atlases, pools and instances; `compose::styles` layer styles (glow, inner shadow, bevel); `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::Composer::beatsOf` and `units`; `compose::Element::mark` selection marks; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules; `SkPathBuilder::arcTo` and `skpathutils::FillPathWithPaint`; `geometry::shapes` silhouette generators; `geometry::path::Frame` and `path::Grid`; `geometry::shapes::ticks` and `chords` divisions; `geometry::path::Polyline` resampling and smoothing; `geometry::path::operations` chain and outline effects; `material::sdf` shapes and styles; `material::Recipe` authoring |
| `shipping_forecast` | Per-glyph text tracks; stagger spreads; named style sheet; selector span restyling; path and vertical text; title card; edge decoration | `sketch::kit::Document` content reader; `sketch::kit::titleCard`; `compose::Element::fx` text tracks; `compose::onEdges` and `compose::inset` adaptors; `compose::fx` text effects (`keys`, `scramble`, `sequence`); `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules; `geometry::shapes` silhouette generators |

#### Study · Pattern

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `black_watch` | Woven cloth generator; custom pattern tiles; instanced atlas pool; nearest image paint; board recipe; check table; justified paragraph | `sketch::kit::Document` content reader; `sketch::kit::table`; `compose::instancing` atlases, pools and instances; `compose::test` geometry and pixel checks; `compose::Pattern` tiled fills; `geometry::shapes` silhouette generators; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::skia::Paint::image` sampling options; `material::pattern` woven cloth; `measure::CheckTable` and `measure::check` |
| `cosmati` | Stone recipe; shape-function paths; trimmed strokes; drop shadow; text on arc; bezel frame; legend kit; entrance transitions | `sketch::kit::legend` and `swatchStrip`; `sketch::kit` panel chrome (`backdrop`, `frame`); `geometry::shapes` silhouette generators; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`) |
| `kumiko_asanoha` | Timber recipe bank; strip lap operation; bevel, inner shadow and glow styles; trimmed strokes; group caching; canvas drawing | `sketch::kit::Document` content reader; `compose::styles` layer styles (glow, inner shadow, bevel); `geometry::shapes` silhouette generators; `geometry::path::operations::stripLaps` strip laps; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::Bank` recipe cache |
| `penrose_paving` | Multigrid tiling; polygon inset; stone recipe bank; canvas-program decoration; layered trimmed brush; check table; slot re-render | `sketch::kit::Document` content reader; `sketch::kit::table`; `compose::Brush` shaped layer stacks; `compose::PaintProgram` canvas decorations; `geometry::path::insetPolygon` mitred insets; `geometry::path::multigrid` rhomb tilings; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::Recipe` authoring; `material::Bank` recipe cache; `measure::CheckTable` and `measure::check` |

#### Study · Motion

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `genesis_fire` | Particle emitters; fixed-step ticker; colored vertices; instanced atlas pools; per-glyph text track; meter and swatch kit; pen-hosted guests | `sketch::kit::legend` and `swatchStrip`; `sketch::kit::meter` and `gauge`; `compose::Element::fx` text tracks; `compose::instancing` atlases, pools and instances; `compose::keyedShape` comparable shapes; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules; `motion::physics::Emitter` and `Particles`; `geometry::shapes` silhouette generators; `material::pattern` stock tiles |
| `hitman_verlet` | JSON content; Verlet points and constraints; fixed-step interpolant; instanced atlas pools; chart plots; per-glyph track; layered paints | `sketch::kit::plot` chart layers; `compose::Element::fx` text tracks; `compose::instancing` atlases, pools and instances; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules; `motion::physics::Points` and `Constraint` (Verlet); `geometry::shapes` silhouette generators |
| `slitscan_2001` | Instanced texture-window pools; runtime tone shader; per-glyph track; edge masks; line-decoration presets; haloed text; stock silhouettes; float raster readback; line fit | `compose::Element::fx` text tracks; `compose::instancing` atlases, pools and instances; `compose::Element::mask` and `compose::by` gates; `compose::PaintProgram` canvas decorations; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `compose::lines::presets` and `compose::brush::presets`; `compose::kit::scrim` and `drawHaloed` legibility; `motion::Spread` and `motion::Cascade` schedules; `SkSurfaces::Raster` standalone raster surfaces; `geometry::shapes` silhouette generators; `geometry::shapes` parametric curve generators; `material::pattern` stock tiles; `measure::lineFit` least-squares fit |
| `vertigo_titles` | Harmonograph shapes; brush stroke presets; per-glyph tracks; title card kit; text on path; layered paints; hash noise | `sketch::kit::titleCard`; `compose::Element::fx` text tracks; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `compose::lines::presets` and `compose::brush::presets`; `motion::Spread` and `motion::Cascade` schedules; `core::noise::hash` / `lattice` positional hashes; `geometry::shapes` parametric curve generators |

#### Study · Film

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `eva_magi_defense` | Cap-height metrics; image-filter phosphor; bound image pan; raster bakes; stroke-to-outline paths; chamfered silhouettes; cascade ladder; measured checks; kit table | `sketch::kit::table`; `compose::keyedShape` comparable shapes; `compose::metrics` and `compose::runPens` measurement; `compose::LayeredBrush` stroke layers; `motion::Spread` and `motion::Cascade` schedules; `SkSurfaces::Raster` standalone raster surfaces; `SkCanvas` layers, `drawPaint` and `drawImageRect`; `SkPathBuilder::arcTo` and `skpathutils::FillPathWithPaint`; `geometry::shapes` corner operators; `material::skia::Paint` motion-driven uniforms and offsets; `material::skia::Effect::filter`; `material::skia::Paint::image` sampling options; `measure::CheckTable` and `measure::check` |
| `eva_magi_deliberation` | Cap-height metrics; condensed faces; face weight readback; inset double borders; image-filter phosphor; CRT field recipe; knockout type | `compose::Border` and `compose::decorations::border` rules; `compose::metrics` and `compose::runPens` measurement; `weave::ports::face` with a width style; `SkTypeface` variation axes and style readback; `geometry::shapes` silhouette generators; `material::skia::Effect::filter` |
| `eva_magi_interior` | Cap-height metrics; condensed faces; face weight readback; chamfered shapes; float-field runtime shader with bound uniform; image-filter phosphor; CRT recipe | `compose::Border` and `compose::decorations::border` rules; `compose::metrics` and `compose::runPens` measurement; `weave::ports::face` with a width style; `SkSurfaces::Raster` standalone raster surfaces; `SkShader` local matrices and raw image shaders; `SkTypeface` variation axes and style readback; `geometry::shapes` corner operators; `material::skia::Paint` motion-driven uniforms and offsets; `material::skia::Effect::filter`; `material::skia::Paint` SkShader interop |
| `lain_navi` | Retained slots; runtime shader; CRT field recipe; blurred additive text; blurred additive dashed stroke layers; bound outputs | `compose::keyedShape` comparable shapes; `compose::LayeredBrush` stroke layers |

#### Study · Science

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `chaucer_astrolabe` | Scoped theme; stereographic projection; stock silhouettes; banked latten; speckle; instanced ticks; hatch and taper strokes; edge wipe; check console; charts | `sketch::kit::plot` chart layers; `sketch::kit::Document` content reader; `sketch::kit::table`; `sketch::kit::titleCard`; `compose::instancing` atlases, pools and instances; `compose::keyedShape` comparable shapes; `compose::Element::mask` and `compose::by` gates; `compose::styles` layer styles (glow, inner shadow, bevel); `compose::brush::Ribbon` tapered bands; `compose::feed` rings and text feeds; `compose::lines::Hatch` and `lines::RadialHatch`; `compose::test` geometry and pixel checks; `compose::lines::presets` and `compose::brush::presets`; `compose::instancing::place` placers; `compose::kit::console` and `plate` panels; `geometry::shapes` silhouette generators; `geometry::path::Frame` and `path::Grid`; `geometry::shapes::ticks` and `chords` divisions; `geometry::path::Projection` spherical projections; `material::pattern` stock tiles; `material::kit` grained surfaces (`stone`, `timber`, `latten`, `board`); `material::Recipe` authoring; `material::Bank` recipe cache; `measure::CheckTable` and `measure::check` |
| `chevreul_circle` | Scoped theme; stock silhouettes; instanced tints; text on path; cap-height metrics; edge wipe; charts, tables; readback checks; OCIO effect | `sketch::kit::plot` chart layers; `sketch::kit::Document` content reader; `sketch::kit::table`; `compose::instancing` atlases, pools and instances; `compose::keyedShape` comparable shapes; `compose::Element::mask` and `compose::by` gates; `compose::metrics` and `compose::runPens` measurement; `compose::Wash` material wash; `compose::test` geometry and pixel checks; `geometry::shapes` silhouette generators; `material::ocio` colour management; `measure::CheckTable` and `measure::check` |
| `chladni_tab1` | Instanced flight pool; stock silhouettes; polar frame; contour sampling; radial hatch; type-on track; speckle tile; bound entrances | `compose::Element::fx` text tracks; `compose::instancing` atlases, pools and instances; `compose::lines::Hatch` and `lines::RadialHatch`; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `compose::lines::presets` and `compose::brush::presets`; `motion::Spread` and `motion::Cascade` schedules; `SkContourMeasureIter` path measurement; `geometry::shapes` silhouette generators; `geometry::shapes` parametric curve generators; `geometry::path::Frame` and `path::Grid`; `material::pattern` stock tiles |
| `minard_1869` | Kit charts; px-keyed ribbon bands; double rules; wipe masks; speckle paper; path operators; contour audits; console feeds | `sketch::kit::plot` chart layers; `sketch::kit::table`; `sketch::kit::titleCard`; `compose::keyedShape` comparable shapes; `compose::Element::mask` and `compose::by` gates; `compose::lines::Line` and `lines::Rails`; `compose::brush::Ribbon` tapered bands; `compose::feed` rings and text feeds; `compose::metrics` and `compose::runPens` measurement; `compose::Wash` material wash; `compose::test` geometry and pixel checks; `compose::lines::presets` and `compose::brush::presets`; `compose::kit::console` and `plate` panels; `SkContourMeasureIter` path measurement; `geometry::shapes` silhouette generators; `geometry::path::Profile` width profiles and bands; `geometry::path::Contour` corners, poses and offsets; `geometry::path::Polyline` resampling and smoothing; `material::pattern` stock tiles; `measure::CheckTable` and `measure::check` |
| `nightingale_coxcomb` | Polar band chart; speckle litho materials; per-glyph type-on tracks; text on ring paths; shared needle outputs | `sketch::kit::plot` chart layers; `compose::Element::fx` text tracks; `compose::keyedShape` comparable shapes; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules; `geometry::shapes` silhouette generators; `geometry::path::Frame` and `path::Grid`; `geometry::shapes::ticks` and `chords` divisions; `material::pattern` stock tiles |

#### Study · Esoteric

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `dunhuang_star_chart` | Instanced star atlas; jittered rails and ribbons; spherical projections; polar charts; speckle paper; line fits; document runs; text feeds | `sketch::kit::plot` chart layers; `sketch::kit::Document` content reader; `compose::instancing` atlases, pools and instances; `compose::keyedShape` comparable shapes; `compose::lines::Line` and `lines::Rails`; `compose::Brush` shaped layer stacks; `compose::brush::Ribbon` tapered bands; `compose::feed` rings and text feeds; `compose::Wash` material wash; `compose::lines::presets` and `compose::brush::presets`; `core::noise::hash` / `lattice` positional hashes; `geometry::shapes` silhouette generators; `geometry::shapers` and `path::Shaper`; `geometry::path::Profile` width profiles and bands; `geometry::path::Projection` spherical projections; `material::pattern` stock tiles; `measure::lineFit` least-squares fit |
| `sigillum_aemeth` | Text on path; rails, hatches, stamp brushes; span masks; path crossings; jittered shapes; speckle tile; console feeds; checks | `sketch::kit::Document` content reader; `compose::keyedShape` comparable shapes; `compose::Element::mask` and `compose::by` gates; `compose::lines::Line` and `lines::Rails`; `compose::Brush` shaped layer stacks; `compose::brush` scatter, pattern and art brushes; `compose::PaintProgram` canvas decorations; `compose::feed` rings and text feeds; `compose::lines::Hatch` and `lines::RadialHatch`; `compose::test` geometry and pixel checks; `compose::lines::presets` and `compose::brush::presets`; `compose::kit::console` and `plate` panels; `geometry::shapes` silhouette generators; `geometry::shapers` and `path::Shaper`; `geometry::path` crossings and crossing rules; `material::pattern` stock tiles; `measure::CheckTable` and `measure::check` |
| `thunder_fulu` | Profiled ribbons under span masks; layered brushes, rails, washes; jittered shapes; path displacement, measure; chart, table, console; checks | `sketch::kit::plot` chart layers; `sketch::kit::table`; `compose::keyedShape` comparable shapes; `compose::Element::mask` and `compose::by` gates; `compose::lines::Line` and `lines::Rails`; `compose::Brush` shaped layer stacks; `compose::brush` scatter, pattern and art brushes; `compose::brush::Ribbon` tapered bands; `compose::feed` rings and text feeds; `compose::Wash` material wash; `compose::lines::presets` and `compose::brush::presets`; `compose::kit::console` and `plate` panels; `SkContourMeasureIter` path measurement; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators; `geometry::shapers` and `path::Shaper`; `geometry::path::Profile` width profiles and bands; `geometry::path::Contour` corners, poses and offsets; `geometry::path::Polyline` resampling and smoothing; `material::pattern` stock tiles; `measure::CheckTable` and `measure::check` |

#### Study · Screens

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `cde_motif` | Inherited colour sets; Motif bevels, inset adaptor; pixel styles, sprites; program tile; edge mask; scrollbar; aliased type; checks | `sketch::kit::Document` content reader; `sketch::kit::scrollbar` with `Scrolled`; `compose::Element::mask` and `compose::by` gates; `compose::onEdges` and `compose::inset` adaptors; `compose::Pattern` tiled fills; `compose::styles` rails, scanlines and stipple; `compose::kit::Bevel` and `kit::bevels` presets; `compose::kit` pixel sprites (`Sprite`, `PixelInk`); `core::environment::Provide` / `inheritedOr` values; `geometry::shapes` silhouette generators; `measure::CheckTable` and `measure::check` |
| `spacejam_1996` | Table layout scheme; element snapshots; runtime shaders; element tile; view effect; custom decoration; stock shapes; fixed ticker; checks | `sketch::kit::table`; `compose::PaintProgram` canvas decorations; `compose::Pattern` tiled fills; `compose::Composer::setView` output view; `compose::Table` layout scheme; `geometry::shapes` silhouette generators; `measure::CheckTable` and `measure::check` |
| `twoadvanced_equipment` | Cached HTTPS bitmaps; availability probe; bound envelope motion; kit scrollbar; per-edge strokes; pinned frame layout | `available(why)` probes and `sketch::requireCached`; `sketch::kit::scrollbar` with `Scrolled`; `compose::onEdges` and `compose::inset` adaptors |
| `twoadvanced_v3` | Cached HTTPS and Rive-embedded images; alpha and edge masks; colour-matrix grade; blurred raster bake; per-edge strokes; chamfers; slots | `available(why)` probes and `sketch::requireCached`; `sketch::kit::Document` content reader; `compose::Element::mask` and `compose::by` gates; `compose::onEdges` and `compose::inset` adaptors; `SkSurfaces::Raster` standalone raster surfaces; `image::embeddedPngs` signature scan; `geometry::shapes` corner operators; `material::skia::Effect::filter` |
| `twoadvanced_v4` | HTTPS bitmaps; grid areas; SkSL and SDF materials; pixel styles; bevel, gloss, scrollbar kits; instancing; world bake; chamfers; masks | `available(why)` probes and `sketch::requireCached`; `sketch::kit::scrollbar` with `Scrolled`; `compose::instancing` atlases, pools and instances; `compose::keyedShape` comparable shapes; `compose::Element::mask` and `compose::by` gates; `compose::styles` layer styles (glow, inner shadow, bevel); `compose::onEdges` and `compose::inset` adaptors; `compose::styles` rails, scanlines and stipple; `compose::instancing::place` placers; `compose::kit::Bevel` and `kit::bevels` presets; `compose::kit::aquaGel`, `y2kChrome`, `gloss` finishes; `weave::ports::face` with a width style; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators; `material::sdf` shapes and styles; `material::skia::Paint` motion-driven uniforms and offsets |
| `winamp_base` | Instanced LED and row atlases; skin bevels; marquee kit; program tile; live pen curve; grain materials; ellipsised rows | `compose::instancing` atlases, pools and instances; `compose::keyedShape` comparable shapes; `compose::kit::Bevel` and `kit::bevels` presets; `compose::kit::marquee`; `core::noise::hash` / `lattice` positional hashes; `material::pattern::Tile` programs |

#### Study · Game UI

| Sketch | Needs | Not yet bound |
| --- | --- | --- |
| `astral_tome` | Stock silhouettes; scatter, ribbon and rail strokes; span-masked reveal; bound twinkle; grain leather; document captions | `compose::Element::mask` and `compose::by` gates; `compose::lines::Line` and `lines::Rails`; `compose::Border` and `compose::decorations::border` rules; `compose::brush` scatter, pattern and art brushes; `compose::brush::Ribbon` tapered bands; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators; `geometry::shapes` parametric curve generators; `geometry::path::Profile` width profiles and bands |
| `bg3_dice_roll` | Stock borders, line presets, rails and ornament brushes; stock shapes; mesh face queries; custom canvas die; memoised retained motion | `compose::lines::Line` and `lines::Rails`; `compose::Border` and `compose::decorations::border` rules; `compose::brush` scatter, pattern and art brushes; `compose::brush::Ribbon` tapered bands; `compose::lines::Hatch` and `lines::RadialHatch`; `compose::lines::presets` and `compose::brush::presets`; `geometry::shapes` silhouette generators; `geometry::shapes` corner operators; `geometry::shapes` parametric curve generators; `geometry::mesh` face topology queries |
| `ds2_bench` | Routed connectors; spans mask; layered brushes; kinetic text; instanced atlas; SDF materials; live shader uniforms; stock shapes | `compose::Element::fx` text tracks; `compose::instancing` atlases, pools and instances; `compose::Element::mask` and `compose::by` gates; `compose::connector`, `rail` and routers; `compose::LayeredBrush` stroke layers; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `compose::lines::presets` and `compose::brush::presets`; `compose::instancing::place` placers; `motion::Spread` and `motion::Cascade` schedules; `geometry::shapes` silhouette generators; `geometry::path::Frame` and `path::Grid`; `geometry::shapes::ticks` and `chords` divisions; `material::sdf` shapes and styles; `material::skia::Paint` motion-driven uniforms and offsets |
| `fallout2_charsheet` | Check tables; condensed face matching; bevels and layer styles; decoration adaptors; rounded outlines; keyed shapes; sketch-kit table; unit grid; slots | `sketch::kit::table`; `compose::keyedShape` comparable shapes; `compose::styles` layer styles (glow, inner shadow, bevel); `compose::onEdges` and `compose::inset` adaptors; `compose::kit::Bevel` and `kit::bevels` presets; `weave::ports::face` with a width style; `geometry::shapes` corner operators; `geometry::path::Frame` and `path::Grid`; `measure::CheckTable` and `measure::check` |
| `ksp_mapview` | Instanced starfield; live-bound SDF and globe materials; conic paths; stock and keyed shapes; filament, hatch and tick lines; readout rows | `sketch::kit::Document` content reader; `compose::instancing` atlases, pools and instances; `compose::keyedShape` comparable shapes; `compose::lines::Line` and `lines::Rails`; `compose::LayeredBrush` stroke layers; `compose::lines::presets` and `compose::brush::presets`; `compose::kit::readout` rows; `geometry::shapes` silhouette generators; `geometry::path::Conic` conic sections; `material::sdf` shapes and styles; `material::Recipe` authoring; `material::skia::Paint` motion-driven uniforms and offsets; `material::kit::globe` |
| `psx_doom_fire` | Fixed-step ticker; RGBA image blits; pen text and clipping; retained guest header; kinetic glyph rise; motion cascade | `compose::Element::fx` text tracks; `compose::fx` kinetic presets (`rise`, `pop`, `typeOn`); `motion::Spread` and `motion::Cascade` schedules |
| `thaumonomicon` | Stamped brushes and shapers; hatches; routed connectors; silhouettes; pixel sprites; baked pixel type; lattice hash; grain; bound motion | `compose::keyedShape` comparable shapes; `compose::lines::Line` and `lines::Rails`; `compose::connector`, `rail` and routers; `compose::Brush` shaped layer stacks; `compose::brush` scatter, pattern and art brushes; `compose::PaintProgram` canvas decorations; `compose::lines::Hatch` and `lines::RadialHatch`; `compose::LayeredBrush` stroke layers; `compose::routers` stock routers; `compose::kit` pixel type (`bakeRun`, `Mask`, `PixFont`); `compose::kit` pixel sprites (`Sprite`, `PixelInk`); `SkCanvas` layers, `drawPaint` and `drawImageRect`; `core::noise::hash` / `lattice` positional hashes; `geometry::shapes` silhouette generators; `geometry::path::Frame` and `path::Grid`; `geometry::shapers` and `path::Shaper` |
| `vagrant_story_target` | Device set hosting; lit meshes and lights; compose texture on material slot; frustum extent; baked pixel type; kit meter | Set sketches (`sketch::SetContext`, `describe`); `sketch::kit::meter` and `gauge`; `compose::TextureScene` and `SketchContext::textureScene`; `compose::Border` and `compose::decorations::border` rules; `compose::kit` pixel type (`bakeRun`, `Mask`, `PixFont`); `geometry::mesh::camera::Camera::extentAt`; `material::Texture` and `Material::slot` |
| `xcom_battlescape` | Instanced atlas pools; palette lookup shader; index sprites; coverage pixel type; pattern tiles; check table; lattice hash; composer queries | `sketch::kit::table`; `compose::instancing` atlases, pools and instances; `compose::Pattern` tiled fills; `compose::kit` pixel type (`bakeRun`, `Mask`, `PixFont`); `compose::kit` pixel sprites (`Sprite`, `PixelInk`); `core::noise::hash` / `lattice` positional hashes; `geometry::path::Frame` and `path::Grid`; `material::pattern` stock tiles; `material::skia` ramp shaders; `measure::CheckTable` and `measure::check` |

## Translating a C++ source

Some native spellings translate through an adapter rather than a binding of
the same name. The audit counts a requirement as bound when one of these
covers it:

- Stage metadata uses `sigil.sketch.kit.stage` or the `@sketch` decorator.
- A C++ `SkCanvas` leaf becomes a `compose.pen` leaf drawing through
  `pen.canvas()`; `brush::solid` becomes `compose.stroke(width, paint)`.
- A keyless `compose::pen(program)` or `graphics(program)` leaf becomes a
  keyed `compose.pen(key, program)` or `compose.graphics(key, program)`.
- Bitmap population uses `image.from_rgba`, and pixel read-back uses
  `skia.Image.rgba()`.
- Rectangle sorting uses Python `min` and `max`.
- `material::skia::withAlpha(color, alpha)` becomes
  `material.withAlpha(color, alpha)`.
- `styles::dropShadow` becomes `compose.shadow(color, offset, blur)`, and
  `styles::textGlow(color, sigma)`, which returns an `Effect::glow`, becomes
  `material.Effect.glow(color, sigma)`.
- `Effect::filter(SkImageFilters::Blur(sigmaX, sigmaY, nullptr))` becomes
  `material.Effect.directionalBlur(sigmaX, 0, across=sigmaY)`, which
  builds the same filter.
- `kit::ruby` and `kit::kenten` become the `compose.Annotation` values they
  construct.
- A pattern tile's texture shader becomes `Tile.paint()`.
- A family fallback list in `weave::ports::face` becomes a loop over
  `weave.typeface`, which returns `None` for a missing family; the list needs a
  concrete family last.
- `SketchContext::bakeSet` becomes `world.Scene().render(frame)` followed by
  `Scene.image`.
- `sketch::kit::passage` becomes a hub text read.
- The fixed scheduler is `ctx.ticker.addFixed`, with the native catch-up
  limit, interpolation output and status.
- Optional brush records are copied when read; edits are assigned back to the
  tool.
- A `ticker.timeline()` that holds one output and then ramps it, where that
  output drives a single property, becomes
  `motion.animate(motion.from_(start).to(end), motion.ramp(hold, duration, ease))`
  on that property.

Some bound spellings differ from native, so a translation written with the
native spelling needs the Python one: `Table::group(name)` takes `names=`;
`Bound::map(curve)` and `Bound::wave(shape)` take `function=` and `easing=`;
`mesh::platonic`'s circumradius is `radius=`; `houseFace` takes `italic=` in
place of a slant and cannot request an oblique; `Theme::font` takes `register=`
in its one-argument form and `ink=` for its color; `world::Selection::None` is
`Selection.All`; and `camera.Orbit()` starts at distance 480 where the native
`Orbit` starts at 0.

A texture-scene bake has no adapter that runs before the first frame. Drawing
the scene into a `draw.Graphics` buffer with `pen.element` on the first draw is
an adaptation, so the audit keeps `SketchContext::textureScene` as unbound.

## Verification

The package suites run against the built extension. From `apps/python`, with
the interpreter that built it:

```sh
PYTHONPATH=../spell-circle-canvas/build/python python3 -m unittest discover \
  -s sigil/test -p 'test_*.py'
```

The suites cover:

- `test_pen.py` renders raster sessions through `render_file`: canvas expiry,
  clipping, batched points from iterables, float32 arrays, memoryviews and
  NumPy with dtype, byte-order, shape and stride rejection; offscreen buffers
  that keep pixels across resize, reopen under another host and close after an
  exception; distinct retained guests per call site; color models, dynamic
  dashes and copyable chance streams.
- `test_values.py` and `test_brush.py` cover pixel images and encoding round
  trips, path booleans, SkSL paint uniform copies and recipes, em tracking,
  mesh grids, camera projection and slotted mesh drawing; brush records with
  Python callables, strokes, sampling, fields, polygons, plots, engine state,
  the native brush description round trip, deposition through custom tips and
  bound-method lifetimes.
- `test_compose.py`, `test_motion.py`, `test_kit.py`, `test_runtime.py` and
  `test_callbacks.py` cover memo equality, copying, failure recovery and cycle
  release; shared outputs, binding chains, transitions in seconds, derivation
  and fixed steps; scoped `Provide` across threads and exceptions and memo
  theme capture; grid tracks and caption parts; setup and draw argument
  prefixes; composer queries and session services that reject closed sessions
  and foreign threads.
- `test_authoring.py`, `test_document.py`, `test_weave.py`, `test_data.py` and
  `test_io.py` cover decorator and export rules, children and named inputs;
  document factories and role styles; rich text, paragraph styles, Unicode
  tables, selectors, text paths, inline slots and story overflow; JSON,
  tables, scales and databases; mounted writes, arrivals, feed capacity,
  replay, recordings and UDP replies.
- `test_library_coverage.py` covers database writes, OSC, MIDI and Art-Net
  round trips, schema conversion, lease residency and thread checks, color
  calculations, paragraph editing, UTF-16 query ranges, exclusion flows,
  beside and annotation readings, and headless batched text.
- `test_world.py` checks rendered pixels, frame post-processing and invalid
  graphs, node identity and shared meshes, motion ownership, owned images and
  getters, and checked drawing and thread access.
- `test_capture.py`, `test_cli.py`, `test_environment.py`, `test_launch.py`
  and `test_workspace.py` cover capture moments, the launcher, project
  environments, interpreter compatibility and workspace discovery. The capture,
  launch and workspace cases run only when `SIGIL_TEST_SKETCHBOOK` names a
  Sketchbook.

The native `python_test` binary registers the common bindings without a
sketch runtime and checks World ticker, frame and pixel ownership, Weave layout
and resource lease dependencies (`PythonBindings.WeaveLayoutsAndResourceLeasesOwnTheirDependencies`),
callback release and scope unwinding. The `SketchPython` cases of `sketch_test`
check feed leases, fresh imports and module isolation, reloads after edits,
teardown safety and `REQUIRES` without executing a sketch.

`typing/check_surface.py` requires a named, typed declaration for every public
compiled member and rejects `Any` and collection types without type arguments;
it also checks that every native export of Weave, Data, IO and Material appears
in the public package and its declarations. `typing/generate.py --check` detects
declaration drift; the typing fixtures and several studies are checked in
strict mode; and `packaging/check_wheel.py` installs a wheel offline and
compares its renders. The strict typing checks register only when CMake finds
`basedpyright`, and the drift check only with an interpreter that matches the
extension and has `pybind11-stubgen` 2.5.5.

Every Python study renders headlessly at its declared capture time through the
standalone extension; the two NumPy studies need an interpreter with NumPy.
Inspected by eye, the five ports and the two Draw adaptations match their
native plates in composition; no test compares a study with its native plate.
The botanical adaptation keeps the native leaf, pigment, branch and berry
data, changes some vein details, adds a footer and captures at 0 seconds
instead of 0.25. The liquid-layer adaptation is 960 by 700 pixels at 0.8
seconds, against 720 by 560 at 0.05 seconds.

These checks establish the exercised contracts. They do not establish pixel
identity with the C++ originals, device rendering, or behaviour of bound
surfaces no Python test reaches: pointer and key input and loop control;
corner radii, curves, contours, `shape`, `vertices`, fitted materials and
`NoiseField`; `decodeAsset` and the JPEG, WebP and EXR encoders;
`assembleBrush` and the Photoshop and Procreate decoders; pattern tiles,
`material.Effect`, dither and `geometry.arrange`; World selectors and most pass
options; `SharedMemoryWriter` and every transport except UDP; `layoutWarichu`,
the vertical, path and line-set flows, `ParagraphBuilder` and
`MarkerSet.applyPaint` and `applyStyle`; 3D transforms; pen text, `textFont`
and `blendMode`; the composer's `routesAt`, `active`, `dirty` and
`purgeCaches`; the loop index of `pen.element`; mesh generators other than
`grid` and `box`, camera orbits, `faceCamera` and `drawImagePanel`; point and
spot lights; CIELAB, `deltaE` and gamut fitting. The public export
lists of Compose, Draw, Motion, Geometry, Image, Skia, World, Sketch and Core
are not checked.

Catalog validation still has to translate the sketches without a study and
compare native and Python renders at matching sizes, assets, backends and
animation times. A sketch that needs an optional SDK can be compared only where
both builds have it; wheel builds disable the licensed SDKs.
