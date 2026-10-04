# The sketch pass — bringing every sketch onto the reshaped libraries

For an agent that has never seen the conversations that produced it. Repository
`/Users/long/REI/ifrit-protocol`, application root `apps/spell-circle-canvas` (every path below is
relative to it unless it starts with `/`). State: `main` at the commit named in
`FAILURES.txt` beside this file, 2026-09-28.

## What the pass is

The API reshape (2026-09-24 → 09-28) rewrote SigilIO, SigilData, SigilMaterial, SigilMotion,
SigilMeasure, SigilMedia (SigilImage and SigilVideo joined), SigilGeometry, SigilWorld, SigilWeave
and SigilCompose in library links that, by the tree's rule, never built the sketches. Every
library, test, bench, doc probe and `SigilReferenceExamples` builds and passes. The keep-going
whole-tree build fails in `SigilSketches` only: **196 objects in 168 sketches** (of 147 single-file
sketches and 78 directory sketches). `Grimoire` cannot link until they compile. This pass brings
EVERY sketch — C++ and `python_*.py` — onto the new vocabulary in one go, then rebases only the
plates named below, each with its cause in the commit.

## (a) Rules

- **Read first:** `apps/spell-circle-canvas/CLAUDE.md` (the tree's rules); `src/sketch/README.md`
  (the sketches' canon) and `src/sketch/kit/README.md` (the sketch kit's). Each library's
  `README.md` beside its code is the canon for the names in the tables below; when this brief and
  a README disagree, the README (compile-checked against the headers) wins.
- **Iterate with the live compiler**, no registry rebuild needed:
  `build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire apps/grimoire/sketches/<stem>[/<stem>].cpp --frame out.png`.
  If it refuses with "framework headers are newer than this host", rebuild Grimoire once. Until
  Grimoire links, compile one object with the tree's syntax check
  (`python3 apps/spell-circle-canvas/sketch-pass/syntaxcheck.py <file>`,
  no build-tree write) or build `SigilSketches` with `-- -k 0`.
- **Build through the slot**, never `cmake --build` directly, and never a Debug build:
  `python3 apps/spell-circle-canvas/sketch-pass/buildslot.py build <targets> --who <name>`
  (it queues behind any other builder and runs detached; `buildslot.py run --who <name> -- <command>`
  for ctest or a sweep; `buildslot.py wait <job>` resumes following a long job).
- **Commit as you go, by explicit path only:** `git add <paths>` then
  `git commit --only -F <message file> -- <paths>`, per sketch or per small group. Never `-a`, `-A`,
  `.`, `stash`, `checkout --`, `reset`, `amend`, `rebase`: another hand may hold uncommitted files in
  this checkout. Do not push. Message in the tree's style (`sketch: <what the sketch now says>`).
- **Comments** state the constraint, never history ("renamed from", "used to"), never citations,
  never measurements. Identifiers are full words; no one-letter parameters.
- **A sketch changes only as much as the vocabulary forces; the picture stays the picture.**
  One pass over every sketch, then the sweep; do not stop to re-verify per sketch.
- **Plates:** everything is byte-identical except the movers in (d). Rebase ONLY a named mover,
  with its cause in the commit. A plate that moves for no stated reason is a defect: file it in
  `apps/spell-circle-canvas/FINDINGS.md` (what the code does, what it evidently meant, what a test
  should assert) and do not rebase it.

## (b) The vocabulary map, old → new, one table per library

Libraries are ordered by how many retired spellings the sketches still hold; rows within a library
by their sites. **Sites** are a grep over `apps/grimoire/sketches/` (C++ and Python, the ignored
`.venv` excluded) for the old spelling: a heuristic that over-counts where the old name is also a
valid one (`sk_sp<SkImage>` compiles once Media's door is included; `SkSize` literals still convert
where a braced `{w, h}` is taken) and reads 0 where a sweep already carried the rename or the old
form is not greppable. A 0 row still states a rule the pass may meet. Line numbers are as of the
link that named them; re-find them before editing.

### Material (871 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 19 (11) | `ctx.assets.shader(ctx.local("x.sksl"))` (`sketch::Assets::shader`, an `sk_sp<SkRuntimeEffect>`) handed to `material::skia::sksl(effect, {{"uName", v}}).set(...)`, `skia::program`, a pen's shader builder or a helper taking the effect | `material::shader(ctx.assets.hub(), ctx.local("x.sksl"), Params{…})` (`<sigilmaterial/program/Shader.h>`), a `material::Material` whose uniforms are the fields of `Params` (a struct with `SIGIL_PARAMETERS` or the schema the Material README shows); `.set(name, v)`/`.bind(name, animatable)` as before; child shaders become `ShaderOptions{.textures = {{"content", {}}}}` filled with `m.slot(...)`. A missing or uncompilable file now paints NOTHING (no magenta checker, no last-good program, nothing in the host's error log — reason written once to stderr). A site that needs the raw `SkRuntimeEffect` (spacejam_1996 `ball`, slitscan_2001 `transfer`, a colour-filter use) reads `ctx.assets.hub().text(uri)` and compiles it itself behind `material::skia` |
| 190 (74) | `material::Paint` handed to Compose (`fill(Paint, box)`, the implicit `Fill(Paint)`, Paint-typed variables and helpers) | a `material::Material` (a colour, a recipe instance, `material::linearGradient(…)`); any other Paint (sksl, image, buffer, blend, raw shader, Paint-typed variable) → `material::skia::base(paint)`. Same for `by::alpha/alphaOut/luma/lumaOut`, `textFx::pass`, `toFill`, `Ribbon::fillMaterial`, `brush::presets::taper/calligraphic`, `kit::grooveRamp`, `Pattern::material()` (`material::skia::paint(m)` gets the paint back, e.g. for `boundOffsetOnly()`), `Rule::inkPaint()` → `Rule::inkMaterial()` |
| 165 (47) | `Paint::linear/radial/conical/sweep`, `linearUnit/radialUnit/glowUnit`, `skia::verticalRamp/unitRamp`, Compose's `linearGradient/radialGradient`, `Paint::linearGradient/radialGradient/conicGradient(` | `material::linearGradient(from, to, stops, {options})`, `radialGradient(centre, radius, stops, {options})`, `conicGradient(centre, stops, {options})` (`<sigilmaterial/paint/Bases.h>`) with `ColorStop{offset, color}` (`ColorStops` also takes plain colours spaced evenly, or a `Ramp`) and `GradientOptions{.units = Box (default) \| Pixels, .extent, .repeat, .focus, .focusRadius, .startDegrees, .endDegrees}`. A gradient placed in pixels MUST say `{.units = GradientUnits::Pixels}` or its plate moves; box-unit extents measure from the box centre. Files spelling `material::…Gradient` without `paint/Bases.h`: aero_desktop, astral_tome, fallout2_charsheet, genesis_fire/Stage.cpp |
| 101 (35) | `.layerStyle(...)`, `LayerStyle`, `LayerStyle::echo(o, c)`, brush styles `Overlay`, `colorOverlay`, `gradientOverlay`, `dropShadow`, `innerGlow`, `textGlow`, `#include <sigilcompose/brush/LayerStyles.h>` | the look is the node's material: `.fill(material)` whose `.effects(...)` carry `Filter::shadow(color, {.blur, .offset, .spread, .inside})` (beneath the fill; no blur = the old echo, re-stamping fill and text; `.inside` = inner shadow/glow; offset 0 + spread = outer glow), `Filter::stroke(color, {.width, .position})`, `Filter::bevel({...})`, and whose `.layer(...)`s are the old overlays. `echo(o, c)` → `.effects(Filter::shadow(c, {.offset = {o.x, o.y}}))` on `fill` (or `ink` for text); `textGlow` → `Filter::glow`. Drop the include; `<sigilmaterial/filter/Filter.h>` where a `Filter` is spelled |
| 88 (36) | `material::skia::toSkColor` / `toColor` reached through the paint header | include `<sigilmaterial/skia/Color.h>`; where the far side now takes a `material::Color` or `glm::vec4` (blend keys, mesh styles, lights) drop the conversion: `glm::vec4{c.r, c.g, c.b, c.a}` |
| 60 (24) | `SkTileMode::kX`, `.filter(SkFilterMode)`, `SkSamplingOptions` | `material::Repeat::X`; `.sampling(material::Sampling::…)`; `imageRendering(material::Sampling)` |
| 40 (37) | `#include <sigilmaterial/core/X.h>`, X ∈ Recipe Program Leaf Bank Terms Combine UniformBlock FrameData | `<sigilmaterial/advanced/X.h>` for its own use (`Recipe`, `FrameData`, `UniformBlock`, `Leaf`, `ProgramCache`, `termsSource`/`skSLFromSlang`, `over`/`under`/`stackDepth`, `Bank`); `core/Material.h` includes none of them |
| 34 (24) | `Paint::recipe(m)`, `Paint::solid(c)` | `m`; `c` — BUT an `.ink(...)` that must override the leaf's own style keeps a paint: `material::skia::base(material::Paint::solid(c))` (a colour ink is the inherited lane) |
| 31 (15) | `.slot("name")` filled later with `m.slot("name", …)` | `material::shader(body, P{…}, {.textures = {{"name", {}}}})`, the same `m.slot(…)`; a plain picture: `{.textures = {{"name", pixels}}}` with `.sampling = material::Sampling::Nearest` when wanted |
| 28 (26) | `std::make_shared<const Recipe>(Recipe::of<P>("key").body(Target::SkSL, body))` + `Material(recipe, P{…})`; recipe accessors (`lattenRecipe()`, `boardRecipe()`, `crtOverlayRecipe()` …) and their statics | ONE call `material::shader(body, P{…}, {.key = "key"})` (`<sigilmaterial/program/Shader.h>`; options `material::ShaderOptions{.key, .target, .sampling, .textures}`); interned by source, key, language, parameter layout and texture names, so calling it in every describe prunes and survives a hot reload — no held `shared_ptr<const Recipe>`, no static. Keep the old recipe name as `.key`. 26 files (list below) |
| 26 (15) | `SkBlendMode::kX` | `material::BlendMode::X` (`kPlus` → `PlusLighter`); `instances(blend)`, `StrokeLayer::blend`, `Scanlines::blend`, `Wash::blend`, `.blendMode(...)`, World's `pass.composite(...)` take it |
| 12 (7) | `.foreground(styles::BevelEmboss{depth, size, angle, hi, lo})` (`.angleDeg`) | `…effects(material::Filter::bevel({.depth, .size, .angleDegrees, .highlight, .shadow}))`; several marks chain `Filter::bevel({…}).then(Filter::shadow(c, {…, .inside = true}))`; y2k_chrome:250, :269 hold a `styles::BevelEmboss bevel;` → `material::BevelOptions bevel;`. Sites: chaucer_astrolabe:288, :295, :566, :586, :613; chladni_tab1:278; kumiko_asanoha:246; kumiko_asanoha/Joinery.h:545; loot_grid/Inventory.h:362; sigillum_aemeth:220 (`.over` list) |
| 12 (4) | `.tile(SkTileMode::kX)`, `Texture::of(img)`, `Texture::produce(k, f)`, `texture.image()`, `.shader()`, `SkIRect` region, texture `size()`, `bevelNormals` | `.tile(material::Repeat::X)`, `Texture(img)`, `Texture(media::PixelSource::produce(k, f))`, `material::skia::image(texture, t)` / `texture.frameAt(t).image`, `material::skia::shader(texture)`, `PixelRect{x, y, width, height}`, `glm::ivec2`, `material::skia::bevelNormals` (`skia/Bevel.h`); pattern programs return `media::PixelSource` (a canvas lambda wrapped in `material::skia::painted(...)`) |
| 10 (5) | `styles::BevelPair{light, dark}`, `compose::shadow({r, g, b, a}, …)` with a braced colour | `light`/`dark`, `Shadow::ink` are `material::Material`: a named `Color` converts, a braced literal does not → `material::Color{0, 0, 0, 0.6f}`. Sites: twoadvanced_v4/Frame.cpp:62, :371, :385, :399, :422; manuscript/Ornament.h:94, ui_particles/GiltBorder.h:81, flourish/flourish.cpp:151, :352 |
| 10 (3) | `skia::Effect::blur/glow/…` | `material::Filter::blur/glow/dropShadow/bloom/brightness/contrast/saturate/hueRotate/crt/phosphorBloom`, chained `.then(...)`; Compose `.filter(...)`/`.backdropFilter(...)` take a `Filter`. `Filter::of(material, SkColorType)` is a TRAP (the colour type reads as a radius) — use `skia::lowered` |
| 8 (5) | `Paint::sksl`, `Paint::image` | `material::shader(...)` / `material::image(pixelSource, {.repeat})`, or `material::skia::base(paint)` (aero_desktop, persona_menu, slitscan_2001) |
| 7 (5) | `styles::Scanlines{c, period, on, phase, blend}` / `styles::scanlines(c, period, on, blend)` | a LAYER of the fill: `material::from(base).layer(material::pattern::scanlines({.color, .period, .on, .phase}), {.blend})`; over the children as the old foreground: `.foreground(decorations::wash(material::pattern::scanlines({…}), blend))`. Sites: cde_motif:505 (+ comment :496), matrix_rain:476, twoadvanced_v4/Hero.cpp:295, :458, Materials.cpp:165, Modules.cpp:44 |
| 7 (5) | `.frame(FrameInput::…)` | nothing: the body spells `uTime`, `uResolution`, `uContentScale`, `uWorld` |
| 7 (4) | `.background(styles::OuterGlow{c, size, spread})` | `…effects(material::Filter::shadow(c, {.blur = size, .spread = spread}))` on the fill's material; sites: coverage_boundary:98 (its under/over lists become the effects chain), ds2_bench:173, :238, kumiko_asanoha:180, twoadvanced_v4/Frame.cpp:105, :114, :180 |
| 6 (5) | `.foreground(styles::InnerShadow{c, off, size})` | `.fill(material::from(base).effects(material::Filter::shadow(c, {.blur = size, .offset = off, .inside = true})))`; sites: black_watch:344, chaucer_astrolabe:414, coverage_boundary:99 (`.over` list), fallout2_charsheet:296, kumiko_asanoha:245, :259 |
| 2 (2) | `compose::hexColor`, `material::rgb(hex, alpha)` | `material::hexColor(0x…)` from `<sigilmaterial/color/Color.h>`; `rgb` is gone |
| 2 (1) | `material::skia::Paint`, `.uniform(name, value)` | `material::Paint` (`paint/Paint.h`, Skia-free; points `glm::vec2`); `.set(name, value)` for constants, `.bind(name, Animatable<float>)` for live values |
| 2 (1) | `Stop` / `RampStop` (`pos`, `position`) | `ColorStop{offset, color}` |
| 2 (1) | `styles::Stipple{c, bits, size, cell}` / `styles::stipple(c, cell)` / `styles::dither(c, on, size, cell)` / `styles::ripple(a, w, p, v)` | `material::pattern::stipple({.color, .bits, .size, .cell})` as a layer or a `decorations::wash`; dither: `.bits = material::pattern::ditherBits(on, size)`; ripple: `material::Filter::of(material::field::ripple(a, w, p, v))`. cde_motif/Motif.h:391 `inline styles::Stipple stipple()` → `inline material::Material stipple() { return material::pattern::stipple({.color = ambient().bg}); }`, each `.overlay(stipple())` → `.layer(stipple(), {})` on the fill or `.overlay(decorations::wash(stipple()))` |
| 1 (1) | `Paint::blend({{A, m}, {B, m2}})`, `.amount(x)` on a layer, `over(base, top, mask, mode)` | `material::from(A).layer(B, {.blend = m2})`; `.opacity = x`; `from(base).layer(top, {.blend = mode, .mask = Mask{.source = mask}})`. Material = base + layers + surface + effects: `material::from(base).layer(source, {.blend, .opacity, .mask = material::Mask{.source, .channel, .low, .high, .invert}}).surface({.metallic, .roughness, .normal, …}).effects(Filter)`; designated `material::Material{{.base, .layers, .surface}}`. Sites left by the scripted sweep (51ae4b413): `material-sketch-open.txt` |
| 1 (1) | `styles::Brackets`/`TickRail` field `color`, `Shadow::color` | `ink` (a Material); positional init unchanged; `TickRail{.color = x}` → `{.ink = x}` (world_hud/Hud.h:227) |
| 0 | a node with NO fill that wore a glow (an image, a text leaf under `decorationOutline`) | `.fill(material::from(material::Color{0, 0, 0, 0}).effects(…))`, or on text `.ink(material::from(ink).effects(…))`; a fill that was `Fill::color(c)` has base `c`, a material fill `material::from(thatMaterial)` |
| 0 | `material::Paint::recipe(Material(recipe)).set(…)`; `material::program(hub, uri, P{…}, {.slots})`; `program/Program.h`, `ProgramOptions`, `programRecipe` | `material::skia::paint(material::shader(body, P{…}))` or the Material itself; `material::shader(hub, uri, P{…}, {.textures = {{"slot", {}}}})` (the file is a recipe body, no uniform declarations) |
| 0 | `material::Texture::of(asset, ms)` | `material::Texture(asset)` (sampled at the material's time) |
| 0 | `crtOverlay(float×6)`; 20 zero-argument `*Recipe()` accessors; `stock::warmup`, `material::warmup(requests)`, `everyRecipe`, colormaps | the struct form (in the sketch-owned `CrtOverlay.h`); accessors are file-local or gone; warmups and `everyRecipe` gone |
| 0 | `material::kit::surface/unlit/map/k*Slot` | `material::surface::program/unlit/map/…` (`<sigilmaterial/surface/Surface.h>`); `surface::lower(material)` is what World's fill runs |
| 0 | `field::noise/grain` as a base | `material::noise(f, {.octaves, .seed, .turbulence, .grain, .contrast, .stretch})` (field forms stay tier 3); `material::image(pixelSource, {.repeat})`; `material::skia::base(paint)` bridges an existing Paint |
| 0 | Substance entrances | `material::substance(hub, uri, Generated{…} \| {.inputs, .preset, .seed, .resolution, .outputs})`, vocabulary in `material::sbsar::` (describe, Input, output(material, usage)); a generated `<stem>.substance.h` per `sketches/<stem>/data/*.sbsar` (substance_swatches swept) |
| 0 | consumer alias namespaces `mskia`, `mpattern`, `mat`, `mkit`, `msdf`, `matkit`, `patterns`, `paint`, `ptn`, `stones`, `materialkit` | `material::…` in full |
| 0 | `decorations::wash(Paint, SkBlendMode, amount)` | `decorations::wash(material, material::BlendMode, amount)` |
| 0 | library looks (stone, latten, board, timber, grained, globe, CRT tube, crtBeam, crtOverlay, gold/chrome/glass, studio/sunset skies, SurfaceParameters presets, text paints, girih, aqua/gloss/chrome ramps/y2kChrome, vignette) | sketch-owned copies, bodies byte-identical, in sketch namespaces: `cosmati/Stone.h`, `Latten.h` (+copies: penrose_paving includes cosmati's, over_under, material_slots, chaucer_astrolabe); board → black_watch, dunhuang_star_chart, kumiko_asanoha, over_under (each `GrainNoise.h`); `kumiko_asanoha/Timber.h`; grained four → `slang_portable/`; `ksp_mapview/Globe.h`; `eva_magi_interior/Crt.h` (`evangelion::crtTube`); `lain_navi/CrtBeam.h`; `crt_bloom/`, `ds2_bench/`, `karaoke_wipe/` `CrtOverlay.h`; `shapeworks_lab/Reflections.h`, `Environments.h` (env_faces, env_lanes, mesh_normal_bridge, reflection_lab include them); presets → reflection_lab as `from(colour).surface({…})`; `text_paints/TextPaints.h`; `zellige/Girih.h/.cpp`; `y2k_chrome/Aqua.h`, `Gloss.h`, `ChromeType.h` (rewritten onto Filter effects, not verbatim); vignette/grained copied into their 6 studies |

### Motion (632 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 264 (57) | `Ticker`, `FrameClock`, `PolicyClock`, `ctx.ticker`, `ticker.add(...)`, `engine.add`, `addFixed`, `FixedStatus`, `timeline().apply(&x).then<RampTo>(…).then<Hold>(…)` | `ctx.engine`: `engine.animate(value, tween)`, `engine.timeline().add(value, tween, position)` with `at(d)`, `afterEnd(offset)` (default), `afterPrevious`, `withPrevious`, `atLabel`; `engine.timer(onUpdate, {.stepRate \| .frameRate})`; playback `play pause resume restart reverse seek complete cancel alternate revert onComplete`; `engine.advance()` / `advance(to)`; `ClockPolicy` under `sigilmotion/advanced/` |
| 87 (17) | `animate(from(a).to(b), ramp(delay, ms, ease))`, `through(...)`, `Sequence`, `ramp()`, `Transitioned` | `motion::animate(Tween<T>{.from = a, .to = b, .duration = 320ms, .delay = 0ms, .ease = ease::outQuad})`; `.from` named = an entrance on mount; `.to` alone = ease whenever `.to` changes; keyframes `.keyframes = {{.to, .duration, .ease}, …}`. A bare `animate({...})` needs the `motion::` prefix (a braced argument gets no argument-dependent lookup) |
| 80 (20) | `Ms`-suffixed fields, float-seconds arguments (`Verlet::timeStep`, springs, `phase(time, period)`, particles) | `std::chrono` everywhere (`380ms`, `1.2s`, `std::chrono::duration<double>(seconds)`); `.count()` only at a numeric boundary |
| 51 (23) | `Spread{.eachMs, .amountMs, .from}`, `.staggerChildren(...)`, a text track's `.stagger` | `stagger(...)` as a VALUE on the child's own verb: `.opacity(animate({.from = 0, .to = 1, .delay = stagger(40ms, {.from = Center})}))`, `.rotate(stagger({-6deg, 6deg}))`; `cues(...)` for cue tables |
| 46 (6) | `physics::Constraint::a` / `::b` | `first` / `second` |
| 41 (14) | `bind(&output).source(a, b).target(c, d).map(ease).pingPong().cosine()…`, `Bound`, `BoundFloat`, `bind/BoundFloat.h` | `motion::bind(value, {.from = {a, b}, .to = {c, d}, .ease, .alternate, .envelope, .quantize, .wrap, .clamp, .wiggle})` — the value, not a pointer; stages are fields |
| 33 (18) | `ch::easeOutQuad` and the other `ch::`/`choreograph::` names | `motion::ease::outQuad` (31 named curves; Back/Elastic/Bounce are factories `ease::outBack()`, `ease::outElastic(amplitude, period)`; families `ease::in(power)`, `out(power)`, `inOut(power)`, `steps(count)`); the curve type is `motion::Easing` (was `EaseFn`). A line that CALLS a curve calls the `Easing` value (`psx_doom_fire.cpp:491,691`, `winamp_base.cpp:176,266`) |
| 14 (7) | `Track::delay`, `Track::duration`, `Track::loop` on a text track | ONE `Track::tween` (`motion::Tween<float>`): `.textFx({.effect = …, .tween = {.duration = 480ms, .delay = motion::stagger(28ms)}, .unit = …})` (designated order: duration before delay). No `.tween` keeps 450ms / stagger(30ms); a tween written whole takes Tween's defaults (no delay, 250ms). `.within` stays on the Track. A LOOPING track `.loop = <period>` → `.tween = {…, .loop = -1, .loopDelay = <period> - <duration>}`; `.alternate` runs every other cycle backwards (swept in 18 files) |
| 7 (6) | `motion::Transition{d}` / `{.duration = d, .delay = x, .ease = e}` / positional `{d, delay, ease}`; `<sigilmotion/values/Transition.h>` | `motion::Tween<float>{.duration = d, .delay = x, .ease = e}` (designated: a Tween's first fields are from/to); include `<sigilmotion/values/Tween.h>` (for `clamp01` alone `<sigilmotion/ease/Ease.h>`). `Element::transition`, `Rule::transition`, World's `Element::transition` read a Tween's timing only; `transition(Duration)` unchanged |
| 6 (5) | `ch::Output<T>` / `choreograph::Output<T>` | `motion::Animatable<T>`; a live value is `motion::animatable(initial)`; read `.value()`, write by assignment |
| 3 (3) | `Cascade` (motion schedule) | `Schedule` (a `Timing` struct, read through `Beat`); by hand: `Schedule(timingOf(Tween<float>{.duration, .delay}), count)`; `motion::Timing` is only what a `Schedule` is built from (`timingOf(tween, within)`; `.loop` a bool + `.loopDelay` + `.alternate`) |
| 0 | `active()`, `isLive()`, `isAnimated()`, `springMoving`, `Composer::active` | `isRunning()`; `Spring::isSettled()`; `Composer::isRunning()` |
| 0 | umbrella headers `bind/Bind.h`, `schedule/Schedule.h` (as umbrella), `physics/Physics.h`, `schedule/Spread.h` | the feature header in use: `bind/Bound.h`, `schedule/Stagger.h` or `Schedule.h` per the README, `physics/Particles.h`, `ease/Ease.h` |
| 0 | `physics::Vec2`; `v.length()` on a vector | `glm::vec2` (positions, velocities, forces, emitter `at/along/size/aim`, pins, `Neighbourhood::within`); `glm::length(v)` / `glm::dot(v, v)` — never `v.length()` (glm's component count, compiles silently). No crossing from `SkPoint`: `genesis_fire/Simulation.cpp` (lines 13–16, 58) and any `SkPoint` into `Points::add`/`Particles::add`/`pin` convert `{p.fX, p.fY}` |
| 0 | `motion::transitionEqual(a, b)`; `spec.duration` / `spec.delay` off a transition | `motion::tweenEqual(a, b)`; `spec.duration.value()` / `spec.delay.value()` |
| 0 | `Tween::ease` read directly | `tween.easing()` (`ease` defaults EMPTY, read as outQuad). Tween field order: from, to, keyframes, duration, delay, ease, loop, loopDelay, alternate, composition |
| 0 | two floats animated separately, a colour lerped in a timer (optional, not a rename) | ONE engine over every animated value: `engine.animate(value, tween)` / `timeline.add(value, tween, at)` over `Animatable<T>` of a float, `glm::vec2`, `glm::vec3`, `Duration`, `material::Color`, anything `motion::interpolate()` draws a line between (a type's own `interpolate(a, b, amount)` by ADL); a LIST is a collective: `engine.animate(letters, {.to = 1.0f, .delay = stagger(40ms)})` returns a `Timeline` |

### Compose (393 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 86 (23) | `at`, `centerAt`, `rect`, `kit::disc`, `ring`, `dot` with `SkPoint`/`SkRect`; `rect(SkRect::MakeXYWH(x, y, w, h))`; braced `SkPoint{}` | `glm::vec2` / `geometry::path::Rect`: `rect(x, y, w, h)`, `disc(glm::vec2 centre, float radius)`, `glm::vec2{}` (beethoven:160, channel_bind:312) |
| 44 (17) | `Across` / `across()`, `.across = 8` | `band(spine, geometry::path::Profile)`; a number converts to a constant-width profile: `band(spine, 22)`, `.profile = 8` |
| 40 (15) | `PaintContext` `size/rootSize/inkAnchorSize` SkSize, `outline/silhouette/borrowedPath` SkPath, `pointer.at`, `toRoot/inkAnchorToRoot` SkMatrix | `glm::vec2`; `geometry::path::Outline` (`toSk` to draw); `glm::vec2`; `geometry::path::Transform`. 15 files swept textually |
| 33 (10) | `imageRegion(SkRect)`, `pen.element(guest, SkRect)`, custom guest `paintRetained(…, SkRect, …)` | `imageRegion(geometry::path::Rect)` (exact_tangent:172, 184 pass SkRect variables); `pen.element(guest, x, y, w, h[, index])` (Python too); `paintRetained(Pen&, const G&, const geometry::path::Rect&, Slot)` |
| 27 (19) | `<sigilsketch/kit/Rows.h>`, `sketch::kit::Reading`, `Readout{…, ruled}`, `labelRow`, `readout(vector, Readout)` | `<sigilcompose/kit/Rows.h>` (`<sigilsketch/kit/Kit.h>` no longer brings it): `compose::kit::Reading{…}` (same fields); `compose::kit::Rows{.measure, .nameMeasure, .swatchSide, .swatchCorners, .divider = Fill::color(theme().palette.rule)}` (gap/labelGap default 5/10 px — pass `theme().spacing.rowGap/labelGap` where the plate must not move); `compose::kit::reading(reading, rows)`; `compose::kit::readout(std::span<const Reading>, Rows)`. The theme's sheet (`caption` role, `.readout` class, `h2` role) gives the registers |
| 27 (17) | a shape answering `SkPath path(SkSize)`, `(SkSize s)` shape lambdas, `keyedShape`/`pathFigure`/`heldPath` over SkPath | `.shape([](glm::vec2 size) { … return geometry::path::Outline; })`, a scheme's `outline(glm::vec2) const`; `heldPath(Outline)`, `keyedShape(key, Outline(vec2))`, `pathFigure(Outline, bleed)`; an `SkPathBuilder` figure crosses with `geometry::path::fromSk(builder.detach())`. Waiting: eva_magi_defense/DefenseLayout.h:696 and every other `(SkSize s)` lambda |
| 23 (21) | `#include <sigilcompose/kit/Kinetic.h>`, `textFx::Entrance`, `enter({.fromScale, .fadeOver})` | `<sigilcompose/typography/Presets.h>` (entrance in `typography/Entrance.h`, both via `typography/Typography.h`); stock entrances are `motion::Tween<textFx::Displaced>` (450ms / stagger(30ms)): as a track `textFx::entrance(textFx::rise(24), {.unit, .progress})`, as an effect `textFx::enter(textFx::rise(24))`; custom `textFx::entrance({.from = textFx::Displaced{.dy = 26}, .duration = 480ms, .delay = motion::stagger(28ms), .ease = motion::ease::outExpo})`; `Displaced::fadeOver` replaces `Entrance::fadeOver`; `Displaced` grew `opacity` and `axis` after `fadeOver`. cosmati:605 → `enter({.from = textFx::Displaced{.scale = 1.5f, .fadeOver = 0.45f}})` |
| 22 (6) | `kit/Ornament.h`, `kit/Flourish.h`, `kit::drawHaloed`/`HaloedLine`, `drawDiamond`/`drawTaperedSweep`, `makeCarvedFrame` answering SkImage, `appendCubic/appendSpiral` SkPoints, `taperedStroke` SkPath, `starburstOutline` | deleted from the library; sketch-owned copies: `manuscript/Ornament.h/.cpp` (namespace `manuscript`), `nine_slice/Ornament.h/.cpp`, `ui_particles/Ornament.h/.cpp` + `GiltBorder.h/.cpp`, `flourish/Ornament.h/.cpp` + `GiltBorder.h/.cpp` (never `Flourish.cpp`: a case-insensitive disk reads it as `flourish.cpp`). `makeCarvedFrame` answers `std::shared_ptr<const media::Image>`; vec2 point runs; `taperedStroke` answers an Outline (or a geometry band/Profile over a spine); `starburstOutline` was `geometry::shapes::star(spikes, 1 - depth)`; slitscan_2001:211 draws its haloed note with a pen's stroked text or `kit::haloed`; flourish.cpp:111, 116, 457 draw their studs themselves; `kit::Shade::offset` glm::vec2 |
| 16 (4) | `textFx::keys({{at0, m0, ease0}, …, {atN, mN}}, tableEase)`, `textFx::Key{at, modifier, ease}` | `textFx::tween({.from = GlyphModifier{m0}, .keyframes = {{.to = m1, .duration = <share>, .ease = ease0}, …}, .ease = tableEase})`: a keyframe's `duration` is its SHARE of the path (write shares in ms against `.duration`, e.g. `.duration = 1s`, `300ms` for 0.30; unset = equal shares); its `ease` curves the segment ARRIVING at it (move an old `Key::ease` one keyframe later); unset curve = linear. `at0 > 0` → a first hold keyframe `{.to = m0, .duration = <share>}`; `keys({{0, {}}, {1, m}})` → `tween({.to = m})`; `keys({{0, m}, {1, {}}})` → `tween({.from = GlyphModifier{m}})`; `std::vector<textFx::Key>` → `std::vector<motion::Keyframe<GlyphModifier>>`. Sites: daemon_console ×2, elastic_type ×5 (+ its `using Table`), matrix_rain ×3, karaoke_wipe ×2, sketches/README.md |
| 12 (6) | `sketch::kit::Row{cells, swatch, key, ink}` + `sketch::kit::table(rows, Table{…, ruled, headRuled})` | `compose::kit::table(std::span<const std::span<const Utf8>> rows, compose::kit::Table{.columns, .swatches = <one Fill per row>, .keys = <one key per row>, .cellLine = …, .divider, .headRuled})`; a row's `ink` is a four-parameter `cellLine` calling `.ink(colour)` for that row. `sketch::kit::Verdict::columns` is `std::vector<compose::kit::Column>` (source compatible) |
| 10 (6) | `Fill::paint()`, `Fill::shader`/`shaderValue`, `SurfacePaint`, a component property taking an animatable fill | `Fill` holds nothing, a colour, a Material (`Fill::material()` → `const Material*`) or a cascade reference (`currentInk`, `var`); a raw Skia shader goes through `material::skia::staticShader`; verbs `fill(Fill, PaintBox)`, `ink(Fill, PaintBox)`, `textStroke(float, Fill)`; every kit property is a `Fill`; a moving ground goes on the element's own `fill`. `Wash::material` is a Material (thunder_fulu:257, 260 wrap Paints in `material::skia::base(...)`) |
| 9 (5) | kit Sprites `dotSprite/spriteImage/indexImage`, `SpriteSheet::image/rect`, `Sprite::grid`, `PixelInk`, `drawSprite(canvas, …)`; PixelType `Coverage`, `Mask::image`, `Cell::mask`, `Present::shadowOffset`, `kit::draw(canvas, …)`, `kit::blit(canvas, …)` | answer `std::shared_ptr<const media::Image>` (pixfont_dotsprite:97, geo_groups:116, pop_deform:92 hand the still on: `image->frames().front().image` or `compose::image`); `SpriteSheet::rect` a Rect (xcom_battlescape:327); `glm::ivec2` grid; `PixelInk` holds a `draw::Pen&` and a `glm::vec2`; `drawSprite(pen, sprite, glm::vec2, style)`; `std::vector<glm::vec4>` + `planeSize`, `media::Image`, `glm::vec2` (vagrant_story_target:473 `kShadow`); `kit::draw(pen, mask, glm::vec2, present)`, `kit::blit(pen, font, glm::vec2, …)` (pixfont_dotsprite:193, 195) |
| 7 (6) | decoration seam `DecorationScheme::paint(SkCanvas&, ctx)`, `custom()`/`PaintProgram`/`ContourWalk::draw` taking a canvas, `Decoration::bleed/reach` SkVector, `decorations::paintOn(canvas, …)` | the pen first: `paint(sigil::draw::Pen&, const PaintContext&)` (the canvas is `*pen.canvas()`); `glm::vec2`; `decorations::paintOn(pen, ctx, Outline, decoration)`. 34 files swept textually — verify each `custom()`/`foreground` lambda |
| 7 (4) | `Composer::bounds(key)` → SkRect; `TextUnit::rect`, `Beat::rect`, `Scope::Node::bounds/outline`, `Scope::box` | `std::optional<geometry::path::Rect>` (`left/top/right/bottom/width/height/centre()/contains/intersects/united/translated/outset`); `geometry::path::Rect` / `Outline`; `toLocal(vec2\|Outline)`. Sites: draw_with_scope:113-114 (`center()` → `centre()`), volatility_cost:553, hit_slots:279-293 |
| 7 (3) | Router/RailRouter/routeBetween/routeAlong answering SkPath; `bandPointAt`; `routers::orbit(SkPoint)`; Anchor/Tether/pin/Board/Anchored Skia points; `connect::wire` with SkRects | answer `Outline`; `glm::vec2`; `Tether::within` Rect, `Tether::place(Rect, vec2) -> Rect`; `connect::wire(Rects \| span<const vec2>)` (dunhuang_star_chart:276, passive_tree:566 fine) |
| 5 (4) | `.effect = textFx::rise/slide/pop/spinIn/scatter(…)`, `textFx::typeOn()`, `textFx::variableAxisSweep(tag, a, b)` as effects, `.until`/`mix` over them | `textFx::enter(textFx::rise(…))` or `textFx::entrance(…, track)`; `textFx::enter(textFx::typeOn()).until(t)`, `mix(textFx::enter(textFx::typeOn()), …)`; `textFx::waveLoop(…)` unchanged |
| 5 (3) | `instancing::place::grid/ring/repeat` Skia points; `GlyphInfo::rest` (`.x()`); `web(view, SkSamplingOptions)` | `glm::vec2` (braced args unchanged); `glm::vec2` (`.x`); `web(view, material::Sampling)` |
| 5 (3) | `textFx::tint(from, to)` | `textFx::tint({.from = from, .to = to})` (a `motion::Tween<material::Color>`; keyframes allowed; unset curve = smoothstep). Sites: fx_scatter_mix, kinetic_card ×3, karaoke_wipe ×3, sketches/README.md |
| 3 (1) | `Composer::setSize/hitTest/setPointer` with Skia points; `LayoutScheme::place()` answering SkRects; `layouts::Grid::gap`; `kit::tinted(base, named)` | `glm::vec2`; `std::vector<geometry::path::Rect>` (spacejam_1996 reads `table.place()`); `glm::vec2`; takes a `weave::Type` |
| 3 (1) | `Pattern::tile(SkSize)`, `offset(SkPoint)`, `sampling(SkSamplingOptions)`, `tiles::window(SkSize …)` → SkMatrix, `tiles::sliceable(pic)` | `glm::vec2`, `material::Sampling`; `tiles::window(glm::vec2, …)` answers `geometry::path::Transform` (place_repeat_tiles:248 concats `geometry::path::toSk(...)`); `snapshot(tree, fonts, SkSize::MakeEmpty(), {.sliceable = true})` |
| 1 (1) | Draw's `brush::Hatch` quarter turn default, the separate hatch types | ONE hatch `geometry::shapes::Hatch` (`cross`, `inset`; default angle 0 — spell the angle); Draw's `brush::Hatch` is `{pattern, jitter, continuous}`; p5's gradient dial is `gradientTaper()`; Compose's `lines::Hatch` is `{strokeFill, pattern, width, live bindings}`. More than a rename: brush_botanical_study, brush_engine_atlas, python_botanical_study |
| 1 (1) | `sketch::kit::Bars{…optional…}` + `bars(labels, values, bars)`; `bars(dataTable, "label", "value", bars)` | `compose::kit::Bars{…}` + `compose::kit::bars(labels, values, bars)` (plain fields; `rest` defaults to none — state the dimmed track where one was drawn); over a table: `compose::kit::bars(<Utf8 labels from table.column<std::string>("label")>, table.column<double>("value"), bars)` |
| 0 | brush values `Shadow/InnerShadow::offset`, `RadialHatch::centre`, `Slice::filter`, `PathSample position/tangent`, `PathFormat::stampPath`, `Region::rect/oval/path`, `lines::dashGeometry/cornerBrackets/cornerGaps` | `glm::vec2`; `material::Sampling`; `glm::vec2`; Outline; `Region::rect/oval(geometry::path::Rect)`, `Region::path(Outline)`; take and answer Outlines (dash intervals `std::span<const float>`, dash vectors `std::vector<float>`) |
| 0 | `PathFormat::effect = SkX::Make(…)` | `#include <sigilcompose/advanced/PathEffect.h>` and `f.effect = pathEffect(SkX::Make(…))` |
| 0 | `Composer::setClock`, `.staggerChildren`, kit `Kinetic.h` `Curve` enum / `overshoot` | gone: the engine is the host's; see Motion; `motion::Easing` |
| 0 | `sigilcompose/core/Feed.h` | `sigilcompose/kit/Feed.h` (namespace `compose::feed` unchanged; daemon_console swept) |
| 0 | `GeometryOperation`; `brush::restyle(op)` | `geometry::Shaper::incomparable(callable, bleed)`; `brush::restyle` takes a `Shaper` |
| 0 | `draw::brush::Line{from, to}` into `Polygon::intersect` | `polygon.intersect(from, to)` (Python `polygon.intersect(a, b)`). Homonyms kept: `compose::lines::Line` vs `compose::kit::Line`; `compose::Wash` vs `draw::brush::Wash`; `compose::Shape` vs `material::sdf::Shape` vs `draw::brush::Shape` |
| 0 | Compose reached through `<sigilcompose/Compose.h>` for `stroke(width, fill)` and `shadow(ink, offset, blur)` | the umbrella brings the core only: include `<sigilcompose/brush/Decorations.h>` |

### Geometry (344 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 124 (35) | mesh painter/billboards `SkSize viewport`; `render::Light::color`, `MeshStyle::baseColor/ambient` SkColor4f; `MeshStyle::texture`, `BillboardStyle::sprite`, `Environment::levels/…`, `drawImagePanel` image `sk_sp<SkImage>`; `MeshStyle::uvTransform` SkMatrix; `MeshStyle::filter` SkFilterMode | `glm::vec2` (drawMesh, drawPanel, drawImagePanel, drawBackdrop, drawBillboards, cookBillboards, Builder::billboards, `render::Executor` overrides); `glm::vec4`; `media::Picture` (converts from `sk_sp<SkImage>` with `<sigilmedia/advanced/Skia.h>`); `geometry::path::Transform`; `render::Sampling::{Linear, Nearest}` (Python `render.Sampling`). Sites: codec_roundtrip, floating_panels, geo_groups, mesh_generators, mesh_normal_bridge, painter_gpu, pop_billboards, pop_deform, pop_math, pop_order, pop_prims, pop_stamps, shapeworks_lab, usd_roundtrip, yarn_marquee |
| 51 (29) | `arrange::onEllipse(SkPoint, SkVector, radians)` → SkPoint; `onRing(i, n, center, radii, startRadians, sweepRadians, turn)`; `moduleSize(SkSize, cols, rows, SkSize)`; `cellRect(cell, SkSize, SkSize, SkPoint, colSpan, rowSpan)` | `<sigilgeometry/path/Arrange.h>`: `onEllipse(glm::vec2, glm::vec2, radians)` → `glm::vec2`; `onRing(i, n, {.center, .radii, .fromDegrees, .sweepDegrees, .turn})` — DEGREES (`fromDegrees` −90, `sweepDegrees` 360, `turn` Closed; a radian start × `geometry::path::kRadToDeg`); `moduleSize(glm::vec2, cols, rows, glm::vec2 gap = {})` → `glm::vec2` (`.x`, not `width()`); `cellRect(cell, glm::vec2 module, {.gap, .origin, .columnSpan, .rowSpan})` → `path::Rect`. `step`, `along`, `Turn`, `Cell`, `cellAt` unchanged. 31 files (19 onEllipse, 8 onRing, 5 moduleSize, 11 cellRect) |
| 36 (5) | `operations::unite/subtract/intersect/exclude/simplify/offset/roundCorners/chamferCorners/displaceSquare(SkPath…)`, `strips`/`stripOutlines`, `Roughen{…}.apply(skPath)`, `PathOperation` = SkPath function, `chain`, `offsetBy` | same calls on `Outline`s (SkPath overloads in the door); strips answer `Outline` / `std::vector<Outline>` (wrap in `toSk`); `.apply(outline)` answers Outline, Skia form `operations::distort(Roughen{…}, skPath)`; `std::function<Outline(const Outline&)>`. Sites: path_booleans, shapeworks_lab, black_watch, night_network, contour_poses (`path::parallel/displace/cornerWindows`) |
| 22 (17) | `along` + `onEllipse` + `angle·kRadToDeg + 90` to face along a spoke; `atan2(t.y, t.x)·kRadToDeg`, `path::heading(v)` | `arrange::placeOnRing(i, n, ring)` → `Placement{position, headingDegrees}` (or `placeOnEllipse(center, radii, radians)`); `arrange::heading(v)`, `arrange::placeAlong(position, tangent)` |
| 21 (11) | `path::toPath(sampled\|polyline)`, `smoothThrough`, `fitCurve`, `conicPath`, `toPath(segments)` answering SkPath | answer `Outline` (`toPath(segments, FillRule)`; Skia form `toPath(segments, SkPathFillType)`) — wrap in `toSk`. Sites: bg3_dice_roll, chaucer_astrolabe, cosmati/Construction, cosmati, ds2_bench, ksp_mapview, kumiko_asanoha/Joinery.h, minard_1869, nightingale_coxcomb, sigillum_aemeth, thunder_fulu |
| 19 (19) | `#include <sigilgeometry/path/Skia.h>` / `<sigilgeometry/path/StrokeSkia.h>` | `#include <sigilgeometry/advanced/Skia.h>` (the conversions `path::toSk`/`fromSk`/`toSkSize`/`centre` and the SKIA FORM of each operator, same name and namespace, SkPath in and out). 19 files |
| 15 (2) | `blend::Key::path`, `Step::path`, `Options::spine` SkPath; `Key/Step::fill/stroke` SkColor4f | `geometry::path::Outline` (`path::fromSk(...)`); `glm::vec4` straight sRGB (`glm::vec4{c.r, c.g, c.b, c.a}`; drop `toSkColor`); `detail::lerpOklab` glm::vec4; `blend::draw(SkCanvas&, steps)` unchanged. blend_options |
| 14 (11) | distributions `path::uniform/poisson/grid/jittered/blueNoise`; phyllotaxis loops; hand arc-length walks; the 14 `makeTransform(SkMatrix::Translate…)` sites; study projection helpers | `path::distribution::…` (swept); front door `path::points(where, path::random(n) \| poisson(r) \| grid(s) \| radial(n, {…}) \| along(s))`; phyllotaxis `path::points(disc, path::radial(n, {.stepDegrees = 137.508f, .growth = path::Growth::SquareRoot}))` (observable_fibonacci); `outline.pointAt/poseAt`, `points(o, along(s))`; `outline.transformed(path::Transform::translate(…))` or `shape.at(…)`; `path::projection::…` or `projection::custom(key, forward, inverse)` |
| 14 (10) | `shape.path({w, h})`, values called as functions, `.path({2r, 2r}).makeTransform(SkMatrix::Translate(-r, -r))` | `path::toSk(shape.outline({w, h}))`, or `shape.at(centre, r)` → an Outline. Sites: blend_options:109/118/249, contour_poses:86, coverage_boundary:82/88, formation_bands:83, mesh_generators:181, mesh_normal_bridge:100, over_under:71, path_booleans:58/126, pop_stamps:193, shapeworks_lab:93; "does not provide a call operator": chladni_tab1 Figures:211, chladni_tab1:324 |
| 8 (5) | `Camera::viewProjection(SkSize)`, `clipProjection(SkISize)`, `extentAt(…)` (`.fWidth`/`.width()`), `project(point, SkSize)` (`->fX`), `camera::toSkM44` | `viewProjection(glm::vec2)`, `clipProjection(glm::ivec2)`, `extentAt` answers `glm::vec2` (`.x`/`.y`), `project(point, glm::vec2)` → `std::optional<glm::vec2>` (`->x`); `toSkM44` in the door. Sites: vagrant_story_target, world_hud (`kSceneSize.fWidth`), yarn_marquee; Python: a viewport is two numbers, `extentAt`/`project` answer tuples (python_mesh_observatory) |
| 6 (4) | `parametric(f, …)` with `f` answering `SkPoint`; `OutlineFunction` answering SkPath | `f` answers `glm::vec2` (curve_shelf, vertigo_titles, chladni Figures, slitscan Panels); `Callable<Outline(glm::vec2)>` |
| 4 (2) | `SweepStation::position/tangent`, `Crossing::at` (`.fX`), `crossingPatch(…, SkPoint, …)` | `glm::vec2` (`.x`); `crossingPatch(…, glm::vec2 at, …)` (cosmati, crossing_rule; bands: formation_bands) |
| 3 (3) | `copies(symmetry)` → `std::vector<SkMatrix>`; `pop::profile::fromPath(SkPath)`; `curve::project(spline, camera, SkSize, n)` → SkPath | `std::vector<path::Transform>`, `copies(symmetry, Outline)`; `fromPath(Outline)` (pop_stamps); `(spline, camera, glm::vec2, n)` → Outline (mesh_generators, shapeworks_lab) |
| 3 (2) | `path::PolarFrame` / `path::Grid` in Skia (`px()`, `dir()`, `skiaDeg`, `degOf`, `Grid::matrix()`, `path::centred(c, w, h)`) | glm: `centre`/`origin` `glm::vec2`, `at()`/`atPixels()`/`direction()` answer `glm::vec2`, `box()` a `path::Rect`, `screenDegrees()`, `screenSweep()`, `degreesOf()`, `fraction(deg, Winding)`; `Grid::rect` → `Rect`, `Grid::transform()` (a `path::Transform`; `toSk()` for SkMatrix); `path::Rect::centredOn(c, {w, h})`. Sites: chaucer_astrolabe:86, chevreul_circle:134-178, chladni_tab1 Figures:46 + :438, fallout2_charsheet:458, frame_grid:86-111, nightingale_coxcomb:198/641-666, rota_convocationis:60/169, sigillum_aemeth:211-443 |
| 2 (1) | `path::Contour::of(skPath)`, `Contour::lengthOf`, `contour.segment/split` (SkPath), `contour.appendSegment(builder, …)` | `path::contoursOf(skPath)` / `path::lengthOf(skPath)`, or `Contour::of(outline)`; `segment/split` answer Outline (Skia forms `path::segmentOf(contour, a, b)`, `path::splitOf(contour, d)`); `path::appendSegment(builder, contour, a, b, move)` |
| 1 (1) | `Polyline::bounds()`, `path::bounds(lines)`, `Region::bounds()`, `TraceOptions::bounds`, `Region::of(SkPath\|SkRect)` | `path::Rect` (`toSk(rect)`); `Region::of(Outline\|Rect)` (chladni_tab1/Figures.h) |
| 1 (1) | `mesh::extrude(SkPath, {.depth})` | `mesh::extrude(outline, depth)`; new `mesh::fill(outline)`, `mesh::loft(sections)`, `mesh::revolve(outline)` |
| 0 | `Polygon`, `Star`, `TicksShape`/`ArcsShape`/`ChordsShape`; `Circle`, `Annulus`, `Squircle`, `Arc`, `Sector`; `Svg`; `Rounded<T>` | `shapes::Radial` (stock `polygon()`, `star()`); `shapes::Ellipse` (stock `circle()`, `annulus()`, `ring()`, `squircle()`, `arc()`, `sector()`; `Circle{.uniform = true}` → `shapes::ellipse({.uniform = true})`, `.startIndex` → `.start`, `circle(SkPathDirection::kCW, …)` → `circle(path::Winding::OutersClockwise, …)`); `shapes::Fitted`; `Cornered<T>` (`rounded()` unchanged; `shape.cornered(r)`) |
| 0 | `Shaper`/`ShaperScheme` `SkPath shape(const SkPath&)` | `Outline shape(const Outline&) const`; `Shaper::incomparable` takes an Outline callable; `geometry::shapers::*::shape` take and answer Outline |
| 0 | `profile::spans` / `Spans` (a stepped width) | `profile::steps` / `Steps` (minard_1869 swept); Compose's `spans::` is unchanged |

### Media (168 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 73 (33) | a Skia image handed to Media's vocabulary (`Image::of(snapshot)`, `material::Texture(skImage)`, `compose::image(skImage)`, a `PixelSource::produce` lambda returning SkImage), `media::deviceImage`/`DeviceBinding` from `advanced/Device.h` | include `<sigilmedia/advanced/Skia.h>` (an `sk_sp<SkImage>` then stands wherever a `Picture` or `PixelSource` is taken, and a Picture assigns to `sk_sp<SkImage>`); `deviceImage`/`DeviceBinding` are declared there. Files: brush_engine_atlas, coverage_boundary, encode_write, env_faces, env_lanes, eva_magi_defense, exr_channels, field_shelf, floating_panels, flourish/Ornament, flourish, gif_frames, half_float, hub_reload, import_native, material_atlas, material_lab, material_slots, matte_luma, mesh_normal_bridge, net_policy, nine_slice/Ornament, nine_slice, ocio_view, over_under, painter_gpu, pop_stamps, psx_doom_fire, shapeworks_lab, sticker_collection, substance_swatches, tile_map, twoadvanced_v3/Assets, twoadvanced_v3/TwoAdvancedV3.h, twoadvanced_v4/Hero, Materials, TwoAdvancedV4.h, ui_particles/Ornament |
| 42 (17) | `asset->frameAt(ms)`, `animated()`, `totalDurationMs()`, `repetitionCount()`, `frames()[i].durationMs` | `frameAt(std::chrono::duration)` (`pen.millis()` → `std::chrono::duration<double>(pen.millis() / 1000.0)`), `isRunning()`, `duration()`, `repetitions()` (−1 forever), `.duration` |
| 31 (16) | `asset->width()/height()`, `image->size()` as SkISize, `frame.image->…` (SkImage) | `size()` is `glm::ivec2` (`.x`, `.y`; `media::toSk(size)` for Skia's); `frame.image` is a `media::Picture`: `media::toSk(frame.image)` for Skia's image, `.size()`, `.identity()` for pointer identity (video_compositing:131; nine_slice/Ornament.h:77, ui_particles/Ornament.h:96, flourish/Ornament.h:68) |
| 7 (3) | `video::decodeVideo(bytes…)` + `hub.fetch` helpers, `clip->probe()`, `clip->frameAt(seconds, recorder)` + `loopTime`, `video::Playback` handles, `compose::video(clip, …)`, `Encoder::make(Format::Mp4, {…})` | `hub.load<media::Video>(uri, {.cachedFrames = 8})`; `clip->metadata()`/`size()`/`duration()`; `clip->frameAt(elapsed)` (loops forever by default; `Timing{.start, .rate, .loop}`), drawn with `media::deviceImage(frame, canvas.recorder())` or `pen.image(frame, x, y, w, h, Fit)`; `VideoOptions::playback = pool` + `clip->hasFrame()`; `compose::image(clip, Fit)` or `image(media::PixelSource(clip, {.start, .rate}), Fit)` with `.opacity()`/`.blendMode()`; `media::Encoder encoder({…})` (+ `if (!encoder)`), `finish()` → bytes. More than a rename: encode_write, exr_channels, gif_frames, hub_reload, net_policy, nine_slice, sticker_collection, twoadvanced_v3/Assets.cpp, video_compose, video_compositing. `Guest` still exists; `hub.subscribe(uri)` is itself a PixelSource |
| 7 (1) | `make_shared<ImageAsset>(ImageAsset::wrap(x))`, `decodeImage(p, n, opts, hint)`, `decodeChannels`, `probeImage`, `planes.makeImage(…)` | `media::Image::of(x)` (swept); `media::decode<media::Image>(span, opts, hint)`; `decode<media::Channels>`; `probeDocument(std::type_identity<media::Image>{}, span, hint)` or `media::probe(span)`; `planes.image(layer \| media::ChannelPick{…})` |
| 4 (3) | `encodeImage(…)`, `media::encode(*skImage, …)`, `difference(*a, *b)`, `coverageMask(*skImage)`, `encoder.append(pixmap)` | `media::encode(…)` → `std::vector<std::byte>` (empty = failure) or `hub.save(uri, image)`; pass the `sk_sp<SkImage>` without `*`, Media's door included (encode_write:100, :124, hub_reload:98, net_policy:64; exr_channels:98 is a channel-plane encode and stays); pixmap forms are declared in `<sigilmedia/advanced/Skia.h>`; `media::append(encoder, pixmap)` / `encoder.append(skImage)` |
| 3 (2) | `ImageAsset`, `image::Frame`/`video::VideoFrame`, `ImageProbe`/`VideoProbe`, `ChannelData`, `DecodeOptions`, `video::EncodeOptions` | `media::Image` (`hub.load<media::Image>` answers `shared_ptr<const Image>`), `media::Frame{image, time, duration, index, device}`, `Metadata` (`duration` chrono, `frames`, `repetitions`), `Channels`, `ImageOptions`/`VideoOptions`, `Encoder::Options`; one `Format{Png, Jpeg, Webp, Exr, Mp4}` |
| 1 (1) | `sigilimage/…`, `sigilvideo/…` includes; `image::`/`video::`/`img::`/`vid::` | `sigilmedia/core/Image.h` (asset), `image/Decode.h`, `image/Channels.h`, `advanced/Resource.h` (decoders), `image/Encode.h`, `video/Video.h`, `video/Encoder.h`; `compose/video/Video.h` gone; `media::` (swept) |

### Weave (154 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 72 (36) | `sk_sp<SkTypeface>` held or assigned from `ports::face(...)`, `ShapingStyle::typeface`, `Type::face`, `defaultFace()`, `LabelOptions::typeface`, `makeStyle(…, typeface)`, `tracked(typeface, …)`; `face.get()`; `face->x()` | `weave::Face` (assigning either way needs `<sigilweave/advanced/Skia.h>`); `face.identity()`; `weave::borrowSk(face)->x()`. Sketch kit: `Theme::face`, `sheet.type.sans/mono` are `weave::Face`. Seen: fx_scatter_mix, pixfont_dotsprite, live_settling, spacing_passes, optical_kerning; 36 files spell `sk_sp<SkTypeface>` |
| 60 (17) | `ports::face/pickTypeface`, `kit::houseFace` with `SkFontStyle` (`::Italic()`, `::Bold()`, `kItalic_Slant`); `FontContext::familyTypeface(family, SkFontStyle)`; `resolveTypeface`, `variedTypeface…`, `FallbackResolver` | a `FaceStyle` (`{.slant = weave::FaceSlant::Italic}`, `{.weight = 700}`) or `(weight, weave::FaceSlant)`; `kit::houseFace(voice, weight, weave::FaceSlant)`; take/answer `Face` (psx_doom_fire, winamp_base/Settings.h, genesis_fire/Instrument.h) |
| 17 (8) | a sketch building a Skia paint for text: `PaintStyle::foreground`, `PaintLayer::paint`, `PaintLayer(SkColor, …)`, `Decoration::paint`, `.paint.addUnderlay/addOverlay` | text looks are said with `.ink(material)` or the cascade; Compose lowers the Material and hands Weave the paint (owner decision 30). Weave's paint model stays the renderer's paint by design (unchanged: those names, `PaintStyle(SkColor)`, `Decoration::color`, `ParagraphLayout::draw/drawBatched`, `PositionedRun::blob`, `wordBlob`, `makeFont`, `faceMetrics`, `FontContext(sk_sp<SkFontMgr>, …)`, `ports::systemFontManager()`, `qt/`, `testing::Plate`) — a sketch site spelling them is rewritten onto `ink(material)` |
| 5 (4) | `weave::kit::dropShadow/glow/outline(SkColor, SkPoint …)`, `kit::makeStyle(size, SkColor, …)`, `LabelOptions::color`, `mixedScriptFiller(…, SkColor×3)`, `kit::drawLabel(…, SkPoint, …)` | `material::Color` (`SkColor4f::FromColor(0x…)`, same floats as `setColor`, or `material::hexColor(0xRRGGBB, a)`), offsets `glm::vec2`; `drawLabel(canvas, context, text, glm::vec2, options)` (psx_doom_fire, vertigo_titles, matrix_rain, persona_menu) |
| 0 | Flow `BlockFlow`/`ExclusionFlow`/`VerticalBlockFlow`/`flowshape::*`/`PathFlow` over `SkRect`/`SkPath`; `LineInterval::origin/direction`, `Exclusion::offset`, `placeAt(… SkPoint*)` | `geometry::path::Rect` (`Rect::of({x, y}, {w, h})`); `flowshape::rectangle/circle/ellipse(Rect)`, `flowshape::path(Outline)`, `flowshape::coverage(media::Picture, Rect, threshold)`; `PathFlow(Outline)`, `addPath(Outline)`; `glm::vec2` (no sketch spells these) |
| 0 | `PositionedRun::origin` (`.x()`/`.fX`), `LineMetrics/ColumnMetrics::rect()`, `glyphOutline()`, `ShapedWord::positions` (`[i].x()`), `ShapedWord::typeface`, `PlacedGlyph::rest/tangent/glyph/color`, `GlyphDress::center/…/matrix/face`, `GlyphRSXformBatches::addGlyph(…, SkPoint, …)` | `glm::vec2` (`.x`); `Rect` (`geometry::path::toSk(...)`); `Outline`; `std::vector<glm::vec2>` (chladni_tab1); `Face`; `glm::vec2`, `uint16_t`, `material::Color` (was 8-bit SkColor); `glm::vec2`, `const geometry::path::Transform*`, `Face`; `glm::vec2` |
| 0 | `RichText::slot(name, SkSize, drop)`, `Run::slotSize`, `PlacedInitial::box/baseline`, `PlacedPlaceholder::rect` (`centerX()`), `Beside::base`, `layoutWarichu(…, SkRect, …)`, `layoutSingleLine/singleLine/testing::layLine(…, SkPoint)`, `PaintLayer::offset` | `glm::vec2`; `geometry::path::Rect` (`centre().x`), `glm::vec2` (no sketch spells these) |
| 0 | `sigilweave/decoration/DecorationRects.h` | `sigilweave/advanced/DecorationRects.h` |

### Data (7 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 5 (1) | `send(address, arguments)` / `reply(address, arguments)`; the seven status getters and `Vitals`; `closed()` | `send(data::oscMessage(address, arguments))`, `send(json, {.to})`, `reply(json)`; `state()` → `ConnectionState` (= `io::FeedState` + `undecodable`); `state().readiness == io::ReadyState::Closed` |
| 2 (2) | `Json::text()`, `items()`, `fields()`, `Kind::{Text, Items, Fields}` | `string()`, `array()`, `object()`, `Kind::{String, Array, Object}` (385 renames in 44 files placed by the compiler); Python `Json.object(pairs)` → `Json.fromPairs`, `JsonKind.String/Array/Object` |
| 0 | `data::Connection(hub, uri, …)` constructors | `data::connect(hub, uri, ConnectOptions{.dialect, .schema, .queue, .capacity})`, `data::replay(hub, uri, recording, …)`, both a copyable `Connection` |
| 0 | handlers taking `const Json& message`, `latest<Value>()`, `latestBytes()` | `const data::Message&`: `message.payload` (the decoded value), `message[key]`, `message[index]` (OSC argument), `number/string/boolean` (first OSC argument), `address()`, `name()`, `sender()`, `arrivedAt()`, `receivedAt()`, `revision()`, `bytes()`, `as<Value>()`; `latest(pattern)` answers a `Message`. `on`/`latest` match OSC 1.0 patterns (`?`, `*`, `[a-c]`, `[!a-c]`, `{a,b}`); `"*"` = every message |
| 0 | `decodeJson/encodeJson/decodeOsc/encodeOsc/decodeMidi/encodeMidi/decodeArtNet/encodeArtNet`, `Osc.h`, `Midi.h`, `ArtNet.h`, SchemaBuffer names | `data::decode(bytes \| text, Dialect, schema)` / `data::encode(json, Dialect, schema)`, `Dialect{Json, Osc, Midi, ArtNet, FlatBuffer, Csv}`; a FlatBuffer at a plain JSON door is `undecodable`: open typed doors with `{.schema}` |
| 0 | hand JSON/CSV/table reads | readers (SigilDataRead): `data::json(hub, uri)`, `data::csv(hub, uri)`, `data::table(hub, uri, {.query})` (a query over a CSV runs in DuckDB with the rows as `source`); `registerDecoders(hub)` lives there and registers `Database` |

### Measure (6 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 6 (3) | `elapsedMs/elapsedUs/reset/toMicroseconds/ScopedMs`, `Samples`+`percentile`, `Check::line`, `totalMs`, `sd`, `binOf`, `centre`, `r2` | one include `<sigilmeasure/Measure.h>`: `Stopwatch` (`elapsed()` → Duration; `restart()`), `timed(callable)`, `summary(range, projection)` → `Summary`, `quantile(range, fraction)`, `Histogram::over(values, {.bins})` / `Histogram{{.low, .high, .bins}}`, `Window{count}` / `Window{{.span = 4s}}`, `Smoothed{over}` / `{{.weight, .timeConstant, .peak}}`, `Rate{4s}`, `CheckTable` + `measure::line(check)`; `explained`; frame lanes, Moments, Quantiles, Rescale, LineFit, Laps, Counters, CheckFormat under `sigilmeasure/advanced/`. Uses to apply: genesis_fire `Smoothed{{.weight = 0.15}}`; hitman_verlet `summary(sticks, stretchOf)` + `Smoothed{{.weight = 0.033}}` (keep the hand peak if the plate must hold); python_live_signals `measure.Window(180)`; feed_vitals `Rate{4s}`; deformed_cloud/nightingale_coxcomb/exr_channels `summary(positions, &vec3::y)` → domain; slitscan_2001 `Stopwatch` + `Milliseconds(...).count()` and two Histograms for the per-bin mean; proof tables from `measure::line(c)` |

### World (5 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 3 (2) | `pass.composite(SkBlendMode::kPlus, o)`, `pass.clear(SkColor4f)`, `levels(…, SkColor4f)`, `frame.extent(SkISize)`, `View::extent`, `Readback::Result::image`, `Sampling{image, uv, filter}` | `material::BlendMode::PlusLighter` (glow_trail:164, :168, set_stagger:166); `material::Color` (an SkColor4f still converts); `glm::ivec2`; `media::Picture`; `Picture`, `glm::mat3`, `material::Sampling` |
| 2 (1) | `world::light::Light`, `LightKind::Sun/Point/Spot`, `direction`, `innerDeg`/`outerDeg`, `color` (glm::vec4), `intensity` (float) | `material::Light`, `material::LightKind::Directional/Point/Spot`; `world::light::travel(light)` / `world::light::aim(light, vector)`; `innerAngle`, `outerAngle`; `material::Color`; `motion::Animatable<float>` (`.value()`); `light::sun/point/spot` keep their spelling (usd_roundtrip) |
| 0 | `world::TransformValues`, `world::localMatrix(values)` | `geometry::mesh::Transform`, `transform.matrix()` |
| 0 | `scene.draw(canvas[, camera])`, `<sigilworld/frame/Targets.h>`, `world::Targets` | `world::draw(scene, canvas[, camera])` and `world::Targets` in `<sigilworld/advanced/Skia.h>` (hit_slots) |

### IO (20 sites)

| Sites (files) | Old | New |
|---:|---|---|
| 9 (8) | `ctx.assets.json(name)` (`sketch::Assets::json`) | `ctx.assets.hub().load<data::Json>(uri)` (`<sigildata/decode/Json.h>`), null when missing. The URI is spelled whole: `ctx.local("x.json")` as before, or `"res://x.json"` where the old call took a bare name — a bare name is now a path from the working directory, not a resource |
| 8 (6) | `ctx.assets.table(name)` (`sketch::Assets::table`) | `ctx.assets.hub().load<data::Table>(uri)` (`<sigildata/table/Table.h>`), same URI rule |
| 1 (1) | `ctx.assets.database(name)` (`sketch::Assets::database`) | `ctx.assets.hub().load<data::Database>(uri)` (`<sigildata/query/Database.h>`), same URI rule |
| 0 | `ctx.assets.problems()`, `beginDeclaration()` | gone from Assets. Failures of any resource now stand on the hub: `hub().problems()` answers `io::Problem{uri, message, line}`; a shader that fails to compile keeps its last good program, a never-compiled one paints `material::placeholder()` (magenta checker), and the host shows the problems as a failed build. `Assets` is the pump and the mount — `hub()`, `poll()`, `mountSketch()`, `root()` |
| 1 (1) | `Arrival` (`.bytes`, `.from`, `.generation`), `feed.newest()`, `feed.generation()` | `Message{payload, sender(), arrivedAt(), receivedAt(), revision()}`; `feed.latest()`; `feed.revision()` |
| 1 (1) | `feed.deliver/fail/replay/advance` from a sketch | the transport's side: `sigilio/advanced/Transport.h` (`Inlet`), `sigilio/testing/Testing.h` (`testing::inletOf(feed).deliver(bytes, arrivedAt)`; pattern in `python_live_signals.py`) |
| 0 | `hub.image(uri, options)` | `hub.load<media::Image>(uri, options)`; a sketch building its OWN `io::Hub` calls `media` and `data` `registerDecoders(hub)` (the host's `Assets` does both) |
| 0 | `hub.feed(uri, FeedPolicy{...})`, `Feed::Policy`, `std::shared_ptr<Feed>`, `feed->…` | `hub.listen(uri, ListenOptions{.capacity = 256, .peer = "..."})` (registers every linked transport on first use); `Feed`, `frames::Publisher`, `frames::Subscription` are VALUE handles read with a dot; a handle held onto a closed door keeps answering the closed door — drop it before reopening |
| 0 | `feed.opened()/closed()/error()/dropped()/address()`, `sendTo(to, bytes)` | `feed.state()` → `FeedState{readiness (ReadyState::Connecting\|Open\|Closed), revision, dropped, localAddress, error; isOpen()}`; `send(payload, {.to = ...})`. More than a rename: feed_vitals, python_live_signals (state branches, `ListenOptions`, `send(to=)`) |
| 0 | mounting a recording onto the live URI; `record({})` as the stop | `hub.replay(uri, recording, ListenOptions = {})` (exact URI match); `feed.record(path)` returns an `io::Recording` that stops when it leaves scope |
| 0 | `registerUdp/registerWs/…` (ten) | `io::registerTransports(hub, {"udp", …})` (empty = all linked) |
| 0 | `hub.probe(uri)`, `hub.write(uri, …)` forms, `TextCatalog`, `Hub::channels`, `hub.fetch(uri)` | `io::probe<ResourceInfo>(hub, uri)`; `hub.write(uri, span)` is the one form; gone; gone; `hub.read(uri)` (also ByteSource/AnyByteSource/ArchiveSource) |
| 0 | `io::Bytes` members `->bytes->bytes`, `.bytes.data()` | `->data()`, `->size()`, `->span()`, `->asText()`; build with `Bytes(std::vector<std::byte>)` or `Bytes(span)` |
| 0 | `hub.dispatch(seconds)` / `dispatch()` / `onDispatch` / `DispatchLease`, host `Assets::dispatch`, `Wires::dispatch` | tier 3 free functions over `io::Hub&` under `sigilio/advanced/`: `io::advance(hub, std::chrono)` / `onAdvance` / `Lease` (`Time.h`); `mount/resolve/select/poll` (`Places.h`); `ResourceLease/retain/preload/discardUnretained` (`Residency.h`); `ResourceInfo/registerDecoder/probe` (`Decoding.h`, found by ADL from generic templates — call unqualified there); `feeds` (`Feeds.h`); `NetworkCache(dir).put(...)` (`Network.h`, replaces `probeNetworkCache/seedNetworkCache`); `registerTransport/transport` (`Transport.h`); `Lease.h` |
| 0 | `io::publish`, `createPublisher(name, Backend, device)`, `frames::subscribe(name, application, device)`, `newestFrame()`, `generation()/standing()`, `OpenedFeed` | `io::frames` (`sigilio/frames/`, SigilIOFrames, Python `sigil.io.frames`); `hub.publish("syphon://NAME", PublishOptions = {})` → `frames::Publisher`, `publisher.send(Frame{texture, commandBuffer, width, height})`; `hub.subscribe("syphon://NAME", SubscribeOptions = {})` → `frames::Subscription`, `subscription.latest()` → `std::optional<Frame>`, `.state()`; `TransportEnd`. `io::Hub hub(HubOptions{.mounts = {{"res://", dir}}, .transports, .network})` |

## Further facts the tables do not carry

- Every node has `stroke(material, {.width, .position})`. Drawn-ornament bundles are
  `DecorationStack{{…}}` (doubleBorder, railwayCarto) passed to `stroke`/`foreground`;
  `kit::connect`'s `Dressing::style` is `std::vector<Decoration>`.
- Compose's `.shape()` takes geometry values unchanged; `path::toSk`/`fromSk` cross Outline ↔ SkPath
  in Geometry's door.
- Compose's umbrella `<sigilcompose/Compose.h>` brings the core only; `stroke(width, fill)` and
  `shadow(ink, offset, blur)` need `<sigilcompose/brush/Decorations.h>`; the rows need
  `<sigilcompose/kit/Rows.h>`.
- Measure uses already swept: cde_motif (`measure::line`), slitscan_2001 (`advanced/LineFit.h`,
  `fit.explained`).
- Library state the pass will meet as it is (not the pass's to change): Data still has
  `scale::linear`/`scale::band` as `Transform`, no `Json` literal, no `Json::as<Value>()`, no
  `advanced/` split (Schema, FlatBuffer, values/ stay), and still offers `decodeCsv`,
  `tableFromJson`, `Engine::Duck` and the host's `Assets::json/table/database`.

## Site lists the tables refer to

**Shader (`Recipe::of<…>().body(Target::SkSL, …)` → one `material::shader(...)`, keeping the old
recipe name as `.key` so messages read the same)** — 26 files: black_watch/Board.h,
chaucer_astrolabe/Latten.h, cosmati/Latten.h, cosmati/Stone.h, crt_bloom/CrtOverlay.h,
ds2_bench/CrtOverlay.h, dunhuang_star_chart/Board.h, ember_decode/ember_decode.cpp
(`burnRecipe(body)` + `Paint::recipe(Material(recipe)).set(…)` →
`material::shader(body, BurnParameters{kInk, kEmber, {kSweep, kSpeckle, kPatch}}, {.key = "ember.burn"})`),
eva_magi_interior/Crt.h (STAYS), frame_inputs/frame_inputs.cpp (`make(name, body)` →
`material::shader(body, BarsParameters{…}, {.key = name})`, the three `.frame` calls go),
karaoke_wipe/CrtOverlay.h, ksp_mapview/Globe.h, kumiko_asanoha/Board.h, kumiko_asanoha/Timber.h,
lain_navi/CrtBeam.h (`.slot("content")` → `.textures = {{"content", {}}}`), material_slots/Latten.h,
material_slots/Stone.h, over_under/Board.h, over_under/Latten.h, over_under/Stone.h,
shapeworks_lab/Reflections.h (`.slot("normals").slot("env")[.slot("backdrop")]` →
`.textures = {{"normals", {}}, {"env", {}}[, {"backdrop", {}}]}`; the `m.slot(…)` fills stay),
slang_portable/{Board,Latten,Stone,Timber}.h (STAY), text_paints/TextPaints.h
(`textPaintRecipe(name, body)` → `material::shader(body, TextPaintParameters{…}, {.key = name})`).
STAY on `advanced/Recipe.h` (tier 3; only the include path changes): `slang_portable/{Board,Latten,
Stone,Timber}.h` (one recipe with an SkSL AND a Slang body), `eva_magi_interior/Crt.h`
(`.slot("bloom", LayerFilter::Blurred, "uBloomRadius")`, an executor-filled slot),
`kumiko_asanoha/Joinery.h` (`Bank`), every `GrainNoise.h` (`termsSource`/`skSLFromSlang` via
`advanced/Terms.h`), `material_lab.cpp`, `material_slots/material_slots.cpp`,
`over_under/over_under.cpp` (`over()` via `advanced/Combine.h`), `frame_inputs/frame_inputs.cpp`'s
`UniformBlock` (via `advanced/UniformBlock.h`; its three recipes DO move to `shader()`), and
`ksp_mapview/Globe.h` / `shapeworks_lab/Reflections.h` for `advanced/Terms.h`. A sketch that got
`FrameData`, `Recipe` or `UniformBlock` through `core/Material.h` now needs the advanced include;
no plate moves (same program text and uniforms; `.key` pins the name).

**Paint-typed variables and helpers handed to Compose (more than a rename; retype to
`material::Material` or wrap in `material::skia::base(...)`):** blur_falloff.cpp:94-97,
chrome_type.cpp:175, ember_decode.cpp:217/230, matte_luma.cpp:214/278-288,
spacejam_1996/Artwork.h:49, thunder_fulu.cpp:257, twoadvanced_v3/Frame.cpp:107/302,
twoadvanced_v3/Sections.cpp:191, twoadvanced_v4/Frame.cpp:120; helpers returning `material::Paint`
from a Material expression: nightingale_coxcomb:251, p5_attractor_loom:46, p5_fractal_garden:49,
p5_liquid_layers:50, thaumonomicon:238, genesis_fire/Stage.cpp:138, y2k_chrome:342. The sites the
scripted material sweep left are in `material-sketch-open.txt` beside this brief; the files the
motion/IO links named are in `l14-sketches-open.txt`.

**`LayerStyles.h` include only** (drop it): aero_desktop, beethoven, cosmati, nightingale_coxcomb,
passive_tree, penrose_paving, persona_menu, rota_convocationis, zellige, twoadvanced_v3/Settings.h,
twoadvanced_v4/Settings.h, winamp_base/Settings.h, world_hud/Hud.h. Positional
`Brackets`/`TickRail` sites need no change (twoadvanced_v4/{Frame,Hero,Materials,Modules}.cpp).

**`kit/Kinetic.h` include** (→ `typography/Presets.h`; 21): annotated_margin, bousen, chladni_tab1,
cosmati, daemon_console, ember_decode, fx_scatter_mix, genesis_fire/Settings.h,
hitman_verlet/Settings.h, karaoke_wipe, kinetic_card, nightingale_coxcomb, psx_doom_fire,
shipping_forecast, slitscan_2001/Settings.h, tategaki, twoadvanced_v3/Settings.h,
twoadvanced_v4/Settings.h, vertigo_titles, winamp_base/Settings.h, y2k_chrome. textFx sites:
`typeOn` — chladni_tab1, daemon_console, kinetic_card, nightingale_coxcomb; `variableAxisSweep` —
kinetic_card, shipping_forecast; `rise` — annotated_margin, bousen, daemon_console,
genesis_fire/Panels, hitman_verlet/Panels, kinetic_card, psx_doom_fire, shipping_forecast,
slitscan_2001/Stage, tategaki, vertigo_titles, and `src/sketch/kit/README.md`
(`.effect = textFx::rise(16.0f)` → `textFx::enter(…)`); `slide` — kinetic_card, shipping_forecast;
`pop` — kinetic_card, shipping_forecast, vertigo_titles; `spinIn` — kinetic_card; `scatter` —
fx_scatter_mix, kinetic_card; `waveLoop` — kinetic_card (unchanged).

**`motion::Transition`** (6): bg3_dice_roll, chladni_tab1, eva_magi_defense/DefenseLayout.h,
nightingale_coxcomb, vertigo_titles (include only), lane_retarget (`motion::Transition ramp()`
returning `{duration}` positionally and `std::optional<motion::Transition>` — the positional brace
becomes `{.duration = …}`).

**Rows kit** (21 files spell a deleted name or include the deleted header): black_watch,
bound_lane, cascade, channel_bind, data_sources (bars over a data table),
eva_magi_defense/DefenseLayout.h, eva_magi_defense, grid_layouts, hit_slots, hub_reload,
lane_retarget, live_settling, optical_kerning, pop_order, pop_prims, spacejam_1996/Drawing.h,
spacejam_1996, thunder_fulu, tile_map, volatility_cost, warichu_placeholder.

**`geometry::arrange`** (31 files; only onEllipse ×19, onRing ×8, moduleSize ×5, cellRect ×11
move): blend_options (along), brush_botanical_study (onEllipse), brush_live_tutorial (along,
onEllipse ×2), cde_motif (onRing ×2), contour_poses (along), crossing_rule (onRing ×2), frame_grid
(cellAt, cellRect ×2, moduleSize, onRing, step ×2), genesis_fire/Simulation (onEllipse),
gerstner_grid (cellRect), glow_trail (along, onEllipse), key_light (along, onEllipse), lantern_room
(onEllipse), loot_grid/Inventory.h (cellRect ×2, moduleSize), material_atlas (cellAt, cellRect),
noise_shelf (cellRect), p5_flow_field (moduleSize), p5_liquid_layers (along, onEllipse ×2, onRing),
passive_tree (along, onEllipse ×2), scene_surfaces (onEllipse), shipping_forecast (onEllipse),
slitscan_2001/Exposure (onEllipse), stroke_atlas (onEllipse ×2), substance_swatches (moduleSize),
tile_map (Cell ×2, cellAt ×2, cellRect), vagrant_story_target (onEllipse ×2), vertigo_titles
(onEllipse), video_compose (cellAt, cellRect, moduleSize), web_panel (onRing), winamp_base (cellAt),
world_hud/Hud.h (onRing), world_hud (cellRect ×2). Python sketches use none.

**Geometry's door include** (`<sigilgeometry/advanced/Skia.h>`, 19): astral_tome, cosmati,
eva_magi_defense, flourish/GiltBorder, flourish/Ornament, flourish, kumiko_asanoha/Joinery.h,
manuscript/Ornament, nine_slice/Ornament, slitscan_2001, spacejam_1996/Drawing.h, thunder_fulu,
twoadvanced_v4/Settings.h, ui_particles/GiltBorder, ui_particles/Ornament, winamp_base/Settings.h,
y2k_chrome/Aqua.h, y2k_chrome/ChromeType.h, y2k_chrome/Gloss.h.

**Physics `SkPoint` crossings** (more than a rename): genesis_fire/Simulation.cpp lines 13–16, 58;
hitman_verlet/Stage.cpp (`Verlet::timeStep`, `Constraint::first/second`).

## Python

Python sketches follow the same map through `sigil.*` (`python_live_signals.py` is already ported).
What changed in the bindings: `SurfacePaintLike` → `MotionFillLike` (what `Element.fill` takes;
`FillLike` also takes a paint and a material); `motion.Transition(0.2)` → `motion.Tween(duration=0.2)`
or the number `0.2`; `sigil.image`/`sigil.video` → `sigil.media` (`fromRgba`, `decode(media.Image,
data, …)` answering an Image, `Image.frameAt(t).image`, `load`/`save`, `Video`, `Playback`,
`Encoder`, `difference`; `media.encode`, `media.difference`, `Encoder.append` still take a Skia
image); `sigil.io`: `Hub(options)`, `HubOptions`, `NetworkOptions`, `Hub.read`,
`Hub.publish/subscribe`, `io.frames.Frame` (native handles as integers), `Message.receivedAt`;
`sigil.data.connect/replay/Connection/Message/ConnectionState/Dialect/decode/encode/oscMessage/
json/csv/table/matchesAddress` (the seven Python feed sketches can move onto `data.connect`),
`Json.fromPairs`, `JsonKind.String/Array/Object`; `sigil.measure` (`Check.passed`, durations as
seconds; `measure.Window(180)`); `sigil.geometry.shapes.radial/ellipse/fitted/polygon/star/circle/…`,
`sigil.geometry.path.Outline` (`.length()`, `.pointAt()`, `.united()` …), `through`, `offset`,
`band`, `points` with `random/poisson/grid/radial/along`, `Transform` (a Python silhouette class
still answers `path(width, height)` with a skia Path for `pen.shape`); a camera viewport is two
numbers and `extentAt`/`project` answer tuples (python_mesh_observatory); `render.Sampling`, not
`skia.FilterMode`; `world.light.Light`/`LightKind` gone — `material.Light` (read-only `kind`,
`position`, `range`, `innerAngle`, `outerAngle`), `material.LightKind`, `world.light.sun/point/spot`
(spot takes `innerAngle`/`outerAngle`), `world.light.travel(light)`; `compose.Shadow.ink` and
`compose.shadow(ink=…)` take a colour or a `material.Material`; `material.pattern.scanlines(color,
period=4, on=2, phase=0)`, `stipple(color, bits=0b1001, size=2, cell=1)`, `ditherBits(on, size=4)`;
`material.shader(source, {"field": value} | NamedTuple, key=, target=, sampling=, textures=
{"name": image})` and `material.shader(hub, uri, parameters)`; `sigil.sketch.kit.Column` is
`sigil.compose.kit.Column`; `polygon.intersect(brush.Line(a, b))` → `polygon.intersect(a, b)`;
`compose.styles` holds no layer styles; `ctx.assets.json/table/database(uri)` → `ctx.assets.hub().load(data.Json | data.Table | data.Database, uri)`, `ctx.assets.shader(uri)` → `material.shader(ctx.assets.hub(), uri, parameters)`. No Python sketch spells a recipe, a layer style, the rows
kit, `textFx`, `arrange` or `Transition`; python_observable_reaction_diffusion was swept onto
`sigil.media`.

## Where Skia may still show in a sketch

A sketch reaches Skia only through a library's door, included by name:
`<sigilgeometry/advanced/Skia.h>` (`path::toSk`/`fromSk`/`toSkSize`/`centre`, `camera::toSkM44`,
the Skia form of each path operator), `<sigilmedia/advanced/Skia.h>` (`media::toSk`, pixmap
encodes, `deviceImage`, an `sk_sp<SkImage>` standing for a `Picture`/`PixelSource`),
`<sigilworld/advanced/Skia.h>` (`world::draw`, `Targets`), `<sigilweave/advanced/Skia.h>` (a Skia
typeface ↔ `weave::Face`, `toSk`/`fromSk`/`borrowSk`, `flowshape::path(const SkPath&)`),
`<sigilcompose/advanced/PathEffect.h>`, Material's `skia/` headers (`skia::base`, `skia::paint`,
`skia::lowered`, `skia/Color.h`, `skia/Bevel.h`). Compose's README "Where Skia still shows" is the
boundary: host entrances (`Composer::draw/snapshot/intrinsicSize/picture/image/texture`), painter
seams (`TextPainter`, `Stroke.h` resolvers, `clipRegion`), escape hatches (`PathFormat::effect`,
the dash record), bakes inside values, the `Instances.h` atlas leaf, SkSL docs. A sketch should
not reach for `Composer::draw(SkCanvas&)`, `snapshot()`/`intrinsicSize()`, `picture()`/`image()`
taking Skia values, `Instances.h`, `kit/Placers.h` or anything else from Skia through Compose. The
kit fills `kit::grained`, `parchmentFill`, `flourishParchment` still wrap raw Skia shaders and
compare by pointer (no prune on re-describe) — not this pass's to fix.

## (c) The failing files and what each one hits

The keep-going build (`cmake --build build --config Release -- -k 0`) lists 196 failing objects,
all in `SigilSketches`; `FAILURES.txt` beside this brief holds each object's first
error. **84 objects stop at a missing header** (marked **stops at**): clang reports nothing after a
fatal include, so for those the right-hand column — the retired spellings a grep finds anywhere in
that sketch (for a directory sketch, every file in its directory) — is the better list of what
the file needs. Compiler kinds are de-duplicated per object, at most six shown.

Most frequent compiler kinds (objects hitting each): missing header <sigilgeometry/path/Skia.h> (25); missing header <sigilcompose/brush/LayerStyles.h> (23); missing header <sigilcompose/kit/Kinetic.h> (23); conversion: sk_sp<SkTypeface> → std::optional<Face> (15); gone: motion::Engine::add (14); gone: sketch::kit::readout (12); missing header <sigilmaterial/core/Recipe.h> (11); signature: fill (10); gone: material::skia::toSkColor (9); signature: std::span<Reading> (9); conversion: SkSize → glm::vec2 (9); gone: motion::Timeline::apply (9); conversion: glm::vec2 → SkPoint (8); no viable overloaded '=' (8); no viable conversion from returned value of type 'Face' to function re (8); conversion: SkPoint → glm::vec2 (7); gone: glm::vec<2, int>::width (7); conversion: float → Duration (6); gone: geometry::shapes::Radial::path (6); conversion: Duration → double (6); conversion: Face → sk_sp<SkTypeface> (5); too many arguments to function call, expected 3, have 7 (5); signature: each (5); signature: encode (4).

| File | Compiler (first errors, in order) | Retired spellings the file still holds (grep over the sketch) |
|---|---|---|
| `aero_desktop/aero_desktop.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | material::Paint in Compose ×21; layer styles/marks → material effects ×3; ch:: → motion:: ×2; Skia geometry types ×2; engine/timeline ×1; Assets::shader → hub ×2 |
| `annotated_margin.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | millisecond fields → std::chrono ×3; Skia faces → weave::Face ×2; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1; Skia geometry types ×1 |
| `artnet_lights/artnet_lights.cpp` | gone: motion::Timeline::apply | ch:: → motion:: ×1; Skia geometry types ×1 |
| `astral_tome/astral_tome.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×11; Spread/ramp/through → Tween/stagger ×5; material::Paint in Compose ×4; geometry door include ×1; ch:: → motion:: ×1; Skia faces → weave::Face ×1 |
| `axis_ripple.cpp` | gone: motion::Engine::add; conversion: float → Duration | millisecond fields → std::chrono ×1 |
| `beethoven.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | layer styles/marks → material effects ×2; Skia geometry types ×2; millisecond fields → std::chrono ×1 |
| `bg3_dice_roll/bg3_dice_roll.cpp` | **stops at** missing header <sigilmotion/values/Transition.h> | material::Paint in Compose ×7; layer styles/marks → material effects ×2; motion::Transition → Tween ×1; Spread/ramp/through → Tween/stagger ×1 |
| `black_watch/black_watch.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | layer styles/marks → material effects ×7; Skia geometry types ×6; material::Paint in Compose ×4; shader → material::shader ×3; sketch rows kit → compose::kit rows ×1 |
| `blend_options.cpp` | conversion: SkPath → Outline; gone: material::skia::toSkColor; gone: geometry::shapes::Spiral::path; gone: geometry::shapes::Radial::path; gone: geometry::shapes::Ellipse::path | Skia geometry types ×7; Skia colours ×4; shape protocol → Outline(glm::vec2) ×3 |
| `blur_falloff.cpp` | gone: material::Texture::shader; signature: fill; conversion: Paint → Fill | material::Paint in Compose ×10 |
| `border_weave.cpp` | gone: compose::Element::layerStyle | layer styles/marks → material effects ×1; Skia geometry types ×1 |
| `bound_lane.cpp` | **stops at** missing header <sigilmotion/bind/BoundFloat.h> | Skia colours ×1 |
| `bousen.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia colours ×4; millisecond fields → std::chrono ×3; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1; Skia geometry types ×1 |
| `bristle_current.cpp` | gone: motion::Engine::addFixed | engine/timeline ×1 |
| `brush_botanical_study.cpp` | conversion: SkPoint → glm::vec2; signature: hatch | Skia geometry types ×15 |
| `brush_engine_atlas.cpp` | signature: hatch |  |
| `brush_live_tutorial.cpp` | conversion: SkPoint → glm::vec2; conversion: glm::vec2 → SkPoint | Skia geometry types ×5 |
| `card_flip.cpp` | gone: motion::Engine::add; conversion: Duration → double; gone: motion::phase | Skia geometry types ×1 |
| `cascade.cpp` | gone: sketch::kit::readout; signature: std::span<Reading> |  |
| `cde_motif/cde_motif.cpp` | gone: compose::styles::Stipple; gone: compose::styles::stipple; gone: material::skia::toSkColor; no viable conversion from returned value of type 'Face' to function re; conversion: sk_sp<SkTypeface> → std::optional<Face>; reference to type 'motion::Animatable<float>' could not bind to an rva; +4 more | layer styles/marks → material effects ×4; Skia geometry types ×4; Skia colours ×4; Skia faces → weave::Face ×1 |
| `channel_bind/channel_bind.cpp` | gone: motion::Bound; no viable conversion from returned value of type 'Animatable<float>' t; undeclared: geometry; conversion: SkPoint → glm::vec2; gone: sketch::kit::readout; signature: std::span<Reading> | Skia geometry types ×13; Skia colours ×1 |
| `chaucer_astrolabe/chaucer_astrolabe.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia geometry types ×14; layer styles/marks → material effects ×7; material::Paint in Compose ×3; Skia faces → weave::Face ×3; shader → material::shader ×3; engine/timeline ×1 |
| `chevreul_circle/chevreul_circle.cpp` | signature: face; no viable conversion from returned value of type 'Face' to function re; unknown type name 'LayerStyle'; undeclared: LayerStyle; gone: compose::Text::layerStyle; conversion: sk_sp<SkTypeface> → std::optional<Face>; +2 more | Skia faces → weave::Face ×6; layer styles/marks → material effects ×4; material::Paint in Compose ×2; Assets::table → hub ×1 |
| `chladni_tab1/chladni_tab1.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia geometry types ×11; layer styles/marks → material effects ×2; textFx keys/Kinetic.h ×1; motion::Transition → Tween ×1; material::Paint in Compose ×1 |
| `chrome_type.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | material::Paint in Compose ×1; Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `codec_roundtrip.cpp` | conversion: Face → sk_sp<SkTypeface>; conversion: sk_sp<SkTypeface> → std::optional<Face> | Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `contour_poses.cpp` | gone: geometry::shapes::Radial::path; conversion: SkPath → Outline | Skia geometry types ×4; shape protocol → Outline(glm::vec2) ×1 |
| `cosmati/Construction.cpp` | no viable conversion from returned value of type 'Outline' to function; no viable overloaded '=' | Skia geometry types ×13; shader → material::shader ×5; shape protocol → Outline(glm::vec2) ×4; layer styles/marks → material effects ×2; textFx keys/Kinetic.h ×1; geometry door include ×1; engine/timeline ×1; Spread/ramp/through → Tween/stagger ×1 |
| `cosmati/cosmati.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×13; shader → material::shader ×5; shape protocol → Outline(glm::vec2) ×4; layer styles/marks → material effects ×2; textFx keys/Kinetic.h ×1; geometry door include ×1; engine/timeline ×1; Spread/ramp/through → Tween/stagger ×1 |
| `coverage_boundary.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | layer styles/marks → material effects ×10; Skia geometry types ×3; shape protocol → Outline(glm::vec2) ×2; Skia blend/tile/sampling ×1 |
| `crossing_rule.cpp` | too many arguments to function call, expected 3, have 7; gone: material::skia::toSkColor; signature: drawCircle | Skia geometry types ×5; Skia colours ×3 |
| `crt_bloom/crt_bloom.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | shader → material::shader ×3 |
| `curve_shelf.cpp` | signature: parametric | Skia geometry types ×2 |
| `daemon_console.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | material::Paint in Compose ×4; textFx keys/Kinetic.h ×3; Skia geometry types ×3; sketch rows kit → compose::kit rows ×1; ch:: → motion:: ×1; engine/timeline ×1; Skia faces → weave::Face ×1; millisecond fields → std::chrono ×1 |
| `dart_flight.cpp` | conversion: float → Duration |  |
| `data_sources/data_sources.cpp` | gone: sketch::kit::bars |; Assets::table → hub ×1; Assets::database → hub ×1 |
| `decay_step.cpp` | gone: motion::spring; conversion: float → Duration; signature: curve | Skia geometry types ×1 |
| `draw_with_scope.cpp` | gone: geometry::path::Rect::center | Skia geometry types ×3 |
| `ds2_bench/ds2_bench.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | material::Paint in Compose ×10; layer styles/marks → material effects ×5; shader → material::shader ×3; Skia geometry types ×2; Skia blend/tile/sampling ×1 |
| `dunhuang_star_chart/dunhuang_star_chart.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | Skia geometry types ×10; shader → material::shader ×3; material::Paint in Compose ×1; Skia faces → weave::Face ×1; Assets::table → hub ×1 |
| `elastic_type.cpp` | gone: compose::textFx::Key; unknown type name 'Table'; excess elements in scalar initializer; gone: compose::textFx::keys; undeclared: Table; expected ';' after expression; +2 more | textFx keys/Kinetic.h ×9; millisecond fields → std::chrono ×5; Spread/ramp/through → Tween/stagger ×1; Skia geometry types ×1 |
| `ember_decode/ember_decode.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | millisecond fields → std::chrono ×7; material::Paint in Compose ×4; textFx keys/Kinetic.h ×1; shader → material::shader ×1; Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `encode_write.cpp` | signature: encode; value of type 'std::vector<std::byte>' is not contextually convertible; gone: media::Image::decode; type 'std::optional<media::Image>' does not provide a call operator; no viable overloaded '='; operands: std::vector<std::byte>, bool; +2 more | Skia geometry types ×2; media Picture ×2; material::Paint in Compose ×1 |
| `env_faces.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | Skia geometry types ×3; Skia colours ×3; Skia blend/tile/sampling ×2 |
| `eva_magi_defense/eva_magi_defense.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×43; material::Paint in Compose ×3; Skia blend/tile/sampling ×3; sketch rows kit → compose::kit rows ×2; Skia colours ×2; shape protocol → Outline(glm::vec2) ×2; millisecond fields → std::chrono ×2; geometry door include ×1; ch:: → motion:: ×1; motion::Transition → Tween ×1; Spread/ramp/through → Tween/stagger ×1; Skia faces → weave::Face ×1 |
| `eva_magi_deliberation.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | Skia geometry types ×7; layer styles/marks → material effects ×2; Skia faces → weave::Face ×1 |
| `eva_magi_interior/eva_magi_interior.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | Skia geometry types ×23; Skia faces → weave::Face ×19; material::Paint in Compose ×3; text Skia paint → ink(material) ×2; shader → material::shader ×2; Skia colours ×1 |
| `exact_tangent.cpp` | gone: material::Texture::image; conversion: SkRect → geometry::path::Rect; signature: at | Skia geometry types ×12 |
| `exr_channels.cpp` | signature: encode; 'image' is not a class, namespace, or enumeration; gone: media::Channels::makeImage; no viable conversion from returned value of type 'Picture' to function | Skia geometry types ×1; Skia blend/tile/sampling ×1 |
| `fallout2_charsheet/fallout2_charsheet.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | material::Paint in Compose ×8; layer styles/marks → material effects ×7; Skia geometry types ×2; Assets::json → hub ×1 |
| `feed_events/feed_events.cpp` | gone: motion::Timeline::apply; type 'motion::Animatable<float>' does not provide a call operator | ch:: → motion:: ×2; Skia geometry types ×1 |
| `feed_sky/feed_sky.cpp` | unknown type name 'SurfacePaint'; gone: motion::Engine::add; no viable conversion from returned value of type 'Paint' to function r | material::Paint in Compose ×1; Skia geometry types ×1 |
| `field_shelf.cpp` | conversion: (lambda at /Users/long/REI/ifrit-protocol/apps/grimoire/sketches/field_shelf.cpp:70:84) → std::function<Picture ()> | Skia geometry types ×2; Skia colours ×2 |
| `first_light.cpp` | conversion: float → Duration |  |
| `floating_panels.cpp` | conversion: SkSize → glm::vec2; conversion: sk_sp<SkImage> → media::Picture; gone: glm::vec<2, float>::width; gone: glm::vec<2, float>::height | Skia geometry types ×2 |
| `flourish/GiltBorder.cpp` | gone: glm::vec<2, int>::width; missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×36; Skia colours ×10; ch:: → motion:: ×7; geometry door include ×3; material::Paint in Compose ×2; Skia blend/tile/sampling ×2; engine/timeline ×1; shape protocol → Outline(glm::vec2) ×1 |
| `flourish/Ornament.cpp` | gone: glm::vec<2, int>::width; missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×36; Skia colours ×10; ch:: → motion:: ×7; geometry door include ×3; material::Paint in Compose ×2; Skia blend/tile/sampling ×2; engine/timeline ×1; shape protocol → Outline(glm::vec2) ×1 |
| `flourish/flourish.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×36; Skia colours ×10; ch:: → motion:: ×7; geometry door include ×3; material::Paint in Compose ×2; Skia blend/tile/sampling ×2; engine/timeline ×1; shape protocol → Outline(glm::vec2) ×1; Assets::shader → hub ×2 |
| `formation_bands.cpp` | gone: geometry::shapes::Radial::path; gone: material::skia::toSkColor | Skia geometry types ×2; Skia colours ×2; shape protocol → Outline(glm::vec2) ×1 |
| `frame_grid.cpp` | gone: glm::vec<2, float>::fX; gone: glm::vec<2, float>::fY; conversion: glm::vec2 → SkPoint; conversion: glm::vec2 → SkVector; conversion: SkPoint → glm::vec2; too many arguments to function call, expected 3, have 7; +2 more | Skia geometry types ×15 |
| `frame_inputs/frame_inputs.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | shader → material::shader ×6; Skia geometry types ×2 |
| `fx_scatter_mix.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Spread/ramp/through → Tween/stagger ×9; millisecond fields → std::chrono ×2; textFx keys/Kinetic.h ×1; Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `genesis_fire/Panels.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×23; Skia faces → weave::Face ×13; material::Paint in Compose ×9; Skia colours ×7; Spread/ramp/through → Tween/stagger ×2; engine/timeline ×1; textFx keys/Kinetic.h ×1; shape protocol → Outline(glm::vec2) ×1 |
| `genesis_fire/Simulation.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×23; Skia faces → weave::Face ×13; material::Paint in Compose ×9; Skia colours ×7; Spread/ramp/through → Tween/stagger ×2; engine/timeline ×1; textFx keys/Kinetic.h ×1; shape protocol → Outline(glm::vec2) ×1 |
| `genesis_fire/Stage.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×23; Skia faces → weave::Face ×13; material::Paint in Compose ×9; Skia colours ×7; Spread/ramp/through → Tween/stagger ×2; engine/timeline ×1; textFx keys/Kinetic.h ×1; shape protocol → Outline(glm::vec2) ×1 |
| `genesis_fire/genesis_fire.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×23; Skia faces → weave::Face ×13; material::Paint in Compose ×9; Skia colours ×7; Spread/ramp/through → Tween/stagger ×2; engine/timeline ×1; textFx keys/Kinetic.h ×1; shape protocol → Outline(glm::vec2) ×1; Assets::json → hub ×1 |
| `geo_groups.cpp` | conversion: std::shared_ptr<media::Image> → media::Picture |  |
| `gerstner_grid.cpp` | conversion: float → glm::vec2; conversion: int → glm::vec2; gone: motion::Engine::add; conversion: Duration → double; conversion: double → Duration; gone: compose::Element::staggerChildren | engine/timeline ×1; Spread/ramp/through → Tween/stagger ×1; Skia geometry types ×1 |
| `gif_frames.cpp` | signature: probe; gone: media::Image::width; gone: media::Image::height; gone: media::Frame::durationMs; signature: frameFigure; conversion: double → std::chrono::duration<double>; +2 more | millisecond fields → std::chrono ×3; Skia geometry types ×1; Skia blend/tile/sampling ×1 |
| `glow_trail.cpp` | conversion: float → Duration; conversion: glm::vec2 → SkPoint; signature: each; conversion: SkBlendMode → material::BlendMode | Skia blend/tile/sampling ×2; Skia geometry types ×1 |
| `grid_layouts.cpp` | gone: sketch::kit::readout; signature: std::span<Reading> | Skia geometry types ×1 |
| `grpc_watch/grpc_watch.cpp` | gone: motion::Timeline::apply | ch:: → motion:: ×1; Skia geometry types ×1 |
| `guest_picture/guest_picture.cpp` | signature: fitted; signature: custom | Skia geometry types ×7; Skia blend/tile/sampling ×4 |
| `hello.cpp` | gone: motion::Engine::add; signature: sin |  |
| `hit_slots.cpp` | **stops at** missing header <sigilsketch/kit/Rows.h> | Skia geometry types ×8; sketch rows kit → compose::kit rows ×1 |
| `hitman_verlet/Panels.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×129; material::Paint in Compose ×3; textFx keys/Kinetic.h ×1; Skia colours ×1; Spread/ramp/through → Tween/stagger ×1; engine/timeline ×1 |
| `hitman_verlet/Rigs.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×129; material::Paint in Compose ×3; textFx keys/Kinetic.h ×1; Skia colours ×1; Spread/ramp/through → Tween/stagger ×1; engine/timeline ×1 |
| `hitman_verlet/Solver.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×129; material::Paint in Compose ×3; textFx keys/Kinetic.h ×1; Skia colours ×1; Spread/ramp/through → Tween/stagger ×1; engine/timeline ×1 |
| `hitman_verlet/Stage.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×129; material::Paint in Compose ×3; textFx keys/Kinetic.h ×1; Skia colours ×1; Spread/ramp/through → Tween/stagger ×1; engine/timeline ×1 |
| `hitman_verlet/hitman_verlet.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×129; material::Paint in Compose ×3; textFx keys/Kinetic.h ×1; Skia colours ×1; Spread/ramp/through → Tween/stagger ×1; engine/timeline ×1; Assets::json → hub ×1 |
| `hub_reload.cpp` | signature: encode; gone: sketch::kit::readout; gone: media::Image::width; gone: media::Image::height | Skia geometry types ×8; media Picture ×1 |
| `import_native.cpp` | no matching conversion for functional-style cast from 'sk_sp<SkImage>' | Skia geometry types ×1 |
| `ink_units.cpp` | signature: ink | Spread/ramp/through → Tween/stagger ×2; material::Paint in Compose ×2 |
| `karaoke_wipe/karaoke_wipe.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | millisecond fields → std::chrono ×8; Spread/ramp/through → Tween/stagger ×5; shader → material::shader ×3; textFx keys/Kinetic.h ×3 |
| `key_light.cpp` | conversion: glm::vec2 → SkPoint; signature: each | Skia geometry types ×1 |
| `kinetic_card.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia faces → weave::Face ×3; millisecond fields → std::chrono ×2; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1; Skia geometry types ×1 |
| `ksp_mapview/ksp_mapview.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | material::Paint in Compose ×17; Spread/ramp/through → Tween/stagger ×12; Skia geometry types ×10; shader → material::shader ×4; engine/timeline ×1 |
| `kumiko_asanoha/kumiko_asanoha.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia geometry types ×9; layer styles/marks → material effects ×7; shader → material::shader ×6; material::Paint in Compose ×6; geometry door include ×1 |
| `lain_navi/lain_navi.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | Skia geometry types ×10; Skia faces → weave::Face ×6; material::Paint in Compose ×5; shader → material::shader ×2 |
| `lane_retarget.cpp` | gone: motion::Transition; no viable conversion from returned value of type 'std::chrono::millise; gone: sketch::kit::readout; signature: std::span<Reading>; gone: Transition; template argument for template type parameter must be a type; +1 more | motion::Transition → Tween ×2; Spread/ramp/through → Tween/stagger ×2; Skia geometry types ×1 |
| `lantern_room.cpp` | conversion: glm::vec2 → SkPoint; conversion: glm::vec4 → material::Color | Skia geometry types ×1 |
| `live_settling.cpp` | conversion: Face → sk_sp<SkTypeface>; conversion: sk_sp<SkTypeface> → std::optional<Face>; gone: sketch::kit::readout; signature: std::span<Reading> | Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `loot_grid/loot_grid.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | material::Paint in Compose ×12; Skia geometry types ×11; layer styles/marks → material effects ×5; engine/timeline ×1; Skia faces → weave::Face ×1; shape protocol → Outline(glm::vec2) ×1 |
| `manuscript/Ornament.cpp` | signature: material::Material; missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×23; Skia colours ×7; Skia faces → weave::Face ×1; geometry door include ×1; Skia blend/tile/sampling ×1 |
| `manuscript/manuscript.cpp` | signature: material::Material; conversion: sk_sp<SkTypeface> → std::optional<Face>; no viable overloaded '=' | Skia geometry types ×23; Skia colours ×7; Skia faces → weave::Face ×1; geometry door include ×1; Skia blend/tile/sampling ×1 |
| `material_atlas.cpp` | conversion: path::Rect → SkRect; signature: custom | Skia geometry types ×9; Skia colours ×2 |
| `material_lab.cpp` | **stops at** missing header <sigilmaterial/core/Combine.h> | Skia colours ×2; shader → material::shader ×1; Skia geometry types ×1 |
| `material_slots/material_slots.cpp` | **stops at** missing header <sigilmaterial/core/Combine.h> | shader → material::shader ×6; Skia blend/tile/sampling ×6; material::Paint in Compose ×4; Skia geometry types ×3; Skia colours ×3; Assets::shader → hub ×1 |
| `matrix_rain/matrix_rain.cpp` | conversion: unsigned int → material::Color; gone: compose::textFx::keys; gone: motion::Spread; gone: compose::styles::scanlines; signature: fill; gone: motion::Engine::add | millisecond fields → std::chrono ×19; textFx keys/Kinetic.h ×3; Spread/ramp/through → Tween/stagger ×3; text Skia paint → ink(material) ×2; layer styles/marks → material effects ×1; material::Paint in Compose ×1 |
| `matte_luma.cpp` | signature: well; conversion: material::Paint → material::Material; signature: fill | material::Paint in Compose ×6; Skia geometry types ×2 |
| `mawarikomi.cpp` | no viable conversion from returned value of type 'Face' to function re; no viable overloaded '='; conversion: sk_sp<SkTypeface> → std::optional<Face> | Skia geometry types ×1 |
| `mesh_generators.cpp` | gone: material::skia::toSkColor; conversion: SkSize → glm::vec2; gone: geometry::shapes::Radial::path | Skia geometry types ×4; Skia colours ×1; shape protocol → Outline(glm::vec2) ×1 |
| `mesh_normal_bridge.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | Skia geometry types ×3; Skia colours ×1; Skia blend/tile/sampling ×1; shape protocol → Outline(glm::vec2) ×1 |
| `midi_pads/midi_pads.cpp` | gone: motion::Timeline::apply; type 'motion::Animatable<float>' does not provide a call operator | ch:: → motion:: ×3; Skia geometry types ×1 |
| `minard_1869/minard_1869.cpp` | signature: face; undeclared: material | material::Paint in Compose ×1; Skia faces → weave::Face ×1; Assets::table → hub ×1 |
| `net_policy.cpp` | gone: material::skia::toSkColor; signature: encode; gone: media::Image::width; gone: media::Image::height | Skia colours ×3; Skia geometry types ×1; media Picture ×1 |
| `night_network.cpp` | expected namespace name; non-lvalue reference to type 'Timeline' cannot bind to a temporary of ; gone: motion::Timeline::apply; undeclared: ch; member reference base type 'float' is not a structure or union; gone: motion::Engine::add; +6 more | Skia geometry types ×3; layer styles/marks → material effects ×2; ch:: → motion:: ×2; engine/timeline ×2; material::Paint in Compose ×2; shape protocol → Outline(glm::vec2) ×2 |
| `nightingale_coxcomb/nightingale_coxcomb.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia geometry types ×7; layer styles/marks → material effects ×5; Spread/ramp/through → Tween/stagger ×5; millisecond fields → std::chrono ×4; textFx keys/Kinetic.h ×1; motion::Transition → Tween ×1; material::Paint in Compose ×1; Assets::table → hub ×3 |
| `nine_slice/Ornament.cpp` | gone: glm::vec<2, int>::width; missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×8; Skia colours ×6; Skia blend/tile/sampling ×2; geometry door include ×1 |
| `nine_slice/nine_slice.cpp` | gone: glm::vec<2, int>::width; conversion: int → std::chrono::duration<double>; conversion: Picture → sk_sp<SkImage> | Skia geometry types ×8; Skia colours ×6; Skia blend/tile/sampling ×2; geometry door include ×1 |
| `noise_shelf.cpp` | conversion: path::Rect → SkRect | Skia geometry types ×2 |
| `ocio_view.cpp` | gone: material::skia::toSkColor; conversion: (lambda at /Users/long/REI/ifrit-protocol/apps/grimoire/sketches/ocio_view.cpp:68:82) → std::function<Picture ()> | Spread/ramp/through → Tween/stagger ×7; Skia geometry types ×5; Skia colours ×2; Skia blend/tile/sampling ×1 |
| `optical_kerning.cpp` | conversion: Face → sk_sp<SkTypeface>; conversion: sk_sp<SkTypeface> → std::optional<Face>; gone: sketch::kit::Reading; template argument for template type parameter must be a type; gone: sketch::kit::readout | Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `osc_desk/osc_desk.cpp` | gone: motion::Timeline::apply | ch:: → motion:: ×3; Skia geometry types ×1 |
| `over_under/over_under.cpp` | **stops at** missing header <sigilmaterial/core/Combine.h> | shader → material::shader ×8; Skia geometry types ×4; Spread/ramp/through → Tween/stagger ×2; Skia colours ×1; Skia blend/tile/sampling ×1; shape protocol → Outline(glm::vec2) ×1 |
| `p5_attractor_loom/p5_attractor_loom.cpp` | signature: from; no viable conversion from returned value of type 'Material' to functio | material::Paint in Compose ×5; Skia geometry types ×2; Assets::shader → hub ×1 |
| `p5_flow_field/p5_flow_field.cpp` | conversion: glm::vec2 → SkSize | material::Paint in Compose ×6; Skia geometry types ×1; Assets::shader → hub ×1 |
| `p5_fractal_garden/p5_fractal_garden.cpp` | signature: from; no viable conversion from returned value of type 'Material' to functio | material::Paint in Compose ×8; Skia geometry types ×5; Assets::shader → hub ×1 |
| `p5_liquid_layers.cpp` | signature: from; signature: layer; no viable conversion from returned value of type 'Material' to functio; too many arguments to function call, expected 3, have 7; conversion: SkPoint → glm::vec2 | Skia geometry types ×3; material::Paint in Compose ×2 |
| `paint_boxes.cpp` | signature: ink; signature: fill | Spread/ramp/through → Tween/stagger ×3; material::Paint in Compose ×2 |
| `paint_shelf.cpp` | signature: fill; signature: each; conversion: SkPoint → glm::vec2 | material::Paint in Compose ×10; Skia geometry types ×2; Skia colours ×2 |
| `painter_gpu.cpp` | conversion: SkSize → glm::vec2; conversion: sk_sp<SkImage> → media::Picture | Skia geometry types ×2 |
| `paragraph_sheet.cpp` | no viable conversion from returned value of type 'Face' to function re; conversion: sk_sp<SkTypeface> → std::optional<Face> | Skia faces → weave::Face ×3; Skia geometry types ×1 |
| `passive_tree/passive_tree.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia geometry types ×14; material::Paint in Compose ×10; shape protocol → Outline(glm::vec2) ×3; layer styles/marks → material effects ×2; engine/timeline ×1 |
| `path_booleans.cpp` | no matching function for call to object of type 'operations::PathOpera; gone: geometry::shapes::Radial::path; conversion: SkPath → Outline; conversion: Outline → SkPath; gone: geometry::shapes::Ellipse::path | Skia geometry types ×4; shape protocol → Outline(glm::vec2) ×2 |
| `pattern_sequence.cpp` | signature: well; signature: fill | material::Paint in Compose ×1; Skia geometry types ×1 |
| `penrose_paving/penrose_paving.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | layer styles/marks → material effects ×2 |
| `persona_menu/persona_menu.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | material::Paint in Compose ×9; Skia faces → weave::Face ×6; layer styles/marks → material effects ×4; Spread/ramp/through → Tween/stagger ×3; Skia geometry types ×2; Skia colours ×2; text Skia paint → ink(material) ×2; engine/timeline ×1; shape protocol → Outline(glm::vec2) ×1; Assets::shader → hub ×1 |
| `phone_sky/phone_sky.cpp` | gone: motion::Timeline::apply | ch:: → motion:: ×1; Skia geometry types ×1 |
| `pixfont_dotsprite.cpp` | conversion: Face → sk_sp<SkTypeface>; conversion: sk_sp<SkTypeface> → std::optional<Face>; no viable overloaded '='; non-lvalue reference to type 'draw::Pen' cannot bind to a value of unr; gone: material::skia::toSkColor | Skia blend/tile/sampling ×7; Skia geometry types ×3; Skia faces → weave::Face ×2; Skia colours ×2 |
| `place_repeat_tiles.cpp` | conversion: SkISize → glm::vec2 | Skia geometry types ×5 |
| `pop_billboards.cpp` | conversion: SkSize → glm::vec2; no matching function for call to object of type '__libcpp_remove_refer; signature: splat | Skia geometry types ×4 |
| `pop_deform.cpp` | no viable overloaded '=' |  |
| `pop_order.cpp` | conversion: sk_sp<SkImage> → media::Picture; gone: sketch::kit::readout; signature: std::span<Reading> |  |
| `pop_prims.cpp` | gone: sketch::kit::readout; signature: std::span<Reading> | Skia geometry types ×2 |
| `pop_stamps.cpp` | no viable overloaded '='; gone: geometry::shapes::Radial::path | Skia geometry types ×4; shape protocol → Outline(glm::vec2) ×1 |
| `psx_doom_fire.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia faces → weave::Face ×6; millisecond fields → std::chrono ×6; ch:: → motion:: ×2; engine/timeline ×2; Skia colours ×2; text Skia paint → ink(material) ×2; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1 |
| `rich_slot_reserve.cpp` | conversion: SkSize → glm::vec2 | Skia geometry types ×1 |
| `rota_convocationis/rota_convocationis.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | layer styles/marks → material effects ×5; Skia geometry types ×5; material::Paint in Compose ×3 |
| `ruby_kenten.cpp` | no viable conversion from returned value of type 'Face' to function re; no viable overloaded '='; conversion: sk_sp<SkTypeface> → std::optional<Face> | Skia geometry types ×1 |
| `scene_surfaces.cpp` | conversion: glm::vec2 → SkPoint | Skia geometry types ×4 |
| `set_stagger.cpp` | gone: motion::Spread; gone: world::Element::staggerChildren; conversion: SkBlendMode → material::BlendMode | Spread/ramp/through → Tween/stagger ×6; millisecond fields → std::chrono ×4; Skia blend/tile/sampling ×1 |
| `shapeworks_lab/shapeworks_lab.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | Skia geometry types ×6; shader → material::shader ×5; Skia blend/tile/sampling ×2; shape protocol → Outline(glm::vec2) ×1 |
| `shipping_forecast/shipping_forecast.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×10; material::Paint in Compose ×2; textFx keys/Kinetic.h ×1 |
| `sigillum_aemeth/sigillum_aemeth.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | layer styles/marks → material effects ×7; Skia faces → weave::Face ×2; material::Paint in Compose ×1; Assets::table → hub ×1 |
| `slang_portable/slang_portable.cpp` | **stops at** missing header <sigilmaterial/core/Recipe.h> | shader → material::shader ×9; Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `slitscan_2001/Exposure.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×18; Skia blend/tile/sampling ×9; material::Paint in Compose ×4; Skia colours ×4; millisecond fields → std::chrono ×2; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1; geometry door include ×1; engine/timeline ×1 |
| `slitscan_2001/Panels.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×18; Skia blend/tile/sampling ×9; material::Paint in Compose ×4; Skia colours ×4; millisecond fields → std::chrono ×2; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1; geometry door include ×1; engine/timeline ×1 |
| `slitscan_2001/Stage.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×18; Skia blend/tile/sampling ×9; material::Paint in Compose ×4; Skia colours ×4; millisecond fields → std::chrono ×2; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1; geometry door include ×1; engine/timeline ×1 |
| `slitscan_2001/slitscan_2001.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×18; Skia blend/tile/sampling ×9; material::Paint in Compose ×4; Skia colours ×4; millisecond fields → std::chrono ×2; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1; geometry door include ×1; engine/timeline ×1; Assets::json → hub ×1; Assets::shader → hub ×1 |
| `spacejam_1996/spacejam_1996.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×17; material::Paint in Compose ×15; layer styles/marks → material effects ×8; Skia faces → weave::Face ×5; sketch rows kit → compose::kit rows ×2; shape protocol → Outline(glm::vec2) ×2; geometry door include ×1; Skia colours ×1; ch:: → motion:: ×1; engine/timeline ×1; Assets::shader → hub ×3 |
| `spacing_passes.cpp` | conversion: Face → sk_sp<SkTypeface>; conversion: sk_sp<SkTypeface> → std::optional<Face> | Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `sticker_collection.cpp` | undeclared: video; gone: media::Video::probe; conversion: double → std::chrono::duration<double>; signature: drawContained | Skia geometry types ×10 |
| `stock_materials.cpp` | signature: fill | material::Paint in Compose ×9 |
| `stroke_atlas/stroke_atlas.cpp` | no viable conversion from returned value of type 'weave::Face' to func; conversion: sk_sp<SkTypeface> → std::optional<Face>; no viable conversion from returned value of type '(lambda at /Users/lo; conversion: glm::vec2 → SkPoint; conversion: SkBlendMode → material::BlendMode; undeclared: LayerStyle; +5 more | Skia geometry types ×9; shape protocol → Outline(glm::vec2) ×5; Skia faces → weave::Face ×5; layer styles/marks → material effects ×3; Skia blend/tile/sampling ×1 |
| `substance_swatches.cpp` | conversion: Picture → sk_sp<SkImage>; conversion: sk_sp<SkImage> → Picture |  |
| `surface_components.cpp` | **stops at** missing header <sigilcompose/core/SurfacePaint.h> | material::Paint in Compose ×1 |
| `tategaki/tategaki.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Skia geometry types ×3; millisecond fields → std::chrono ×3; Skia faces → weave::Face ×1; Skia colours ×1; textFx keys/Kinetic.h ×1; Spread/ramp/through → Tween/stagger ×1 |
| `text_paints/text_paints.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×11; material::Paint in Compose ×3; shader → material::shader ×2 |
| `text_wrap.cpp` | no viable conversion from returned value of type 'Face' to function re; conversion: sk_sp<SkTypeface> → std::optional<Face> | Skia geometry types ×1; Skia faces → weave::Face ×1 |
| `thaumonomicon/thaumonomicon.cpp` | signature: fill; no viable conversion from returned value of type 'Material' to functio; gone: compose::Element::layerStyle; gone: motion::Engine::add; signature: fmod; conversion: SkPoint → glm::vec2 | Skia geometry types ×11; material::Paint in Compose ×5; layer styles/marks → material effects ×3; shape protocol → Outline(glm::vec2) ×1; Assets::json → hub ×2 |
| `threaded_story.cpp` | no viable conversion from returned value of type 'Face' to function re; conversion: sk_sp<SkTypeface> → std::optional<Face> | Skia faces → weave::Face ×2; Skia geometry types ×1 |
| `thunder_fulu/thunder_fulu.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | material::Paint in Compose ×4; sketch rows kit → compose::kit rows ×3; Skia geometry types ×3; geometry door include ×1; Skia faces → weave::Face ×1; Assets::json → hub ×1 |
| `ticker_lanes.cpp` | gone: motion::Engine::add; conversion: double → Duration; gone: motion::Engine::addFixed; gone: motion::Engine::derive; gone: motion::Timeline::apply; gone: motion::Engine::tick; +1 more | engine/timeline ×2; ch:: → motion:: ×1; Skia geometry types ×1 |
| `tile_map.cpp` | conversion: sk_sp<SkImage> → Picture; gone: motion::Engine::add; conversion: float → Duration; conversion: value_type * → motion::Animatable<float>; signature: each; gone: sketch::kit::readout; +2 more | Skia geometry types ×2; Skia colours ×2; Skia blend/tile/sampling ×1 |
| `twoadvanced_equipment.cpp` | no viable conversion from returned value of type 'Face' to function re; signature: face; conversion: double → Duration; gone: motion::Bound; no viable conversion from returned value of type 'Animatable<float>' t; signature: t; +3 more | Skia geometry types ×1 |
| `twoadvanced_v3/Assets.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia faces → weave::Face ×15; Skia blend/tile/sampling ×9; Skia geometry types ×4; material::Paint in Compose ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; Skia colours ×1 |
| `twoadvanced_v3/Frame.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia faces → weave::Face ×15; Skia blend/tile/sampling ×9; Skia geometry types ×4; material::Paint in Compose ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; Skia colours ×1 |
| `twoadvanced_v3/Sections.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia faces → weave::Face ×15; Skia blend/tile/sampling ×9; Skia geometry types ×4; material::Paint in Compose ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; Skia colours ×1 |
| `twoadvanced_v3/twoadvanced_v3.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia faces → weave::Face ×15; Skia blend/tile/sampling ×9; Skia geometry types ×4; material::Paint in Compose ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; Skia colours ×1 |
| `twoadvanced_v4/Frame.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | layer styles/marks → material effects ×9; material::Paint in Compose ×9; Skia blend/tile/sampling ×9; Skia geometry types ×5; Skia colours ×3; shape protocol → Outline(glm::vec2) ×2; Spread/ramp/through → Tween/stagger ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; Skia faces → weave::Face ×1 |
| `twoadvanced_v4/Hero.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | layer styles/marks → material effects ×9; material::Paint in Compose ×9; Skia blend/tile/sampling ×9; Skia geometry types ×5; Skia colours ×3; shape protocol → Outline(glm::vec2) ×2; Spread/ramp/through → Tween/stagger ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; Skia faces → weave::Face ×1 |
| `twoadvanced_v4/Materials.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | layer styles/marks → material effects ×9; material::Paint in Compose ×9; Skia blend/tile/sampling ×9; Skia geometry types ×5; Skia colours ×3; shape protocol → Outline(glm::vec2) ×2; Spread/ramp/through → Tween/stagger ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; Skia faces → weave::Face ×1 |
| `twoadvanced_v4/Modules.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | layer styles/marks → material effects ×9; material::Paint in Compose ×9; Skia blend/tile/sampling ×9; Skia geometry types ×5; Skia colours ×3; shape protocol → Outline(glm::vec2) ×2; Spread/ramp/through → Tween/stagger ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; Skia faces → weave::Face ×1 |
| `twoadvanced_v4/twoadvanced_v4.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | layer styles/marks → material effects ×9; material::Paint in Compose ×9; Skia blend/tile/sampling ×9; Skia geometry types ×5; Skia colours ×3; shape protocol → Outline(glm::vec2) ×2; Spread/ramp/through → Tween/stagger ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; Skia faces → weave::Face ×1; Assets::json → hub ×1; Assets::shader → hub ×3 |
| `ui_particles/GiltBorder.cpp` | gone: glm::vec<2, int>::width; signature: material::Material; missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×25; Skia colours ×12; geometry door include ×2; Skia blend/tile/sampling ×2; engine/timeline ×2 |
| `ui_particles/Ornament.cpp` | gone: glm::vec<2, int>::width; missing header <sigilgeometry/path/Skia.h> | Skia geometry types ×25; Skia colours ×12; geometry door include ×2; Skia blend/tile/sampling ×2; engine/timeline ×2 |
| `ui_particles/ui_particles.cpp` | gone: glm::vec<2, int>::width; signature: material::Material; conversion: motion::Animatable<float> → float; gone: motion::Engine::addFixed | Skia geometry types ×25; Skia colours ×12; geometry door include ×2; Skia blend/tile/sampling ×2; engine/timeline ×2 |
| `usd_roundtrip.cpp` | gone: world::light::LightKind; gone: material::LightKind::Sun; gone: glm::vec<4, float>::fR; gone: glm::vec<4, float>::fG; gone: glm::vec<4, float>::fB; gone: glm::vec<4, float>::fA; +1 more | World lights/targets ×2; Skia geometry types ×1 |
| `vagrant_story_target.cpp` | conversion: glm::vec2 → SkPoint; conversion: SkVector → glm::vec2; conversion: glm::vec2 → SkSize | Skia geometry types ×4 |
| `vertigo_titles.cpp` | **stops at** missing header <sigilcompose/kit/Kinetic.h> | Spread/ramp/through → Tween/stagger ×17; Skia faces → weave::Face ×5; text Skia paint → ink(material) ×5; material::Paint in Compose ×3; Skia geometry types ×3; millisecond fields → std::chrono ×3; Skia colours ×2; textFx keys/Kinetic.h ×1; motion::Transition → Tween ×1 |
| `video_compose.cpp` | **stops at** missing header <sigilcompose/video/Video.h> | Skia geometry types ×4; Skia blend/tile/sampling ×2 |
| `video_compositing.cpp` | undeclared: video; gone: media::Video::probe; gone: media::Playback::Handle; conversion: double → std::chrono::duration<double>; gone: media::Playback::request; gone: media::Playback::frame; +8 more | Skia geometry types ×16; Skia blend/tile/sampling ×10; media Picture ×2 |
| `volatility_cost.cpp` | gone: motion::Engine::add; conversion: Duration → double; conversion: pointer → motion::Animatable<float>; signature: each; gone: sketch::kit::Readout; gone: sketch::kit::readout; +6 more | sketch rows kit → compose::kit rows ×2; Skia geometry types ×2; engine/timeline ×1 |
| `warichu_placeholder.cpp` | conversion: SkSize → glm::vec2; gone: sketch::kit::readout; signature: std::span<Reading> | Skia geometry types ×3 |
| `web_panel.cpp` | too many arguments to function call, expected 3, have 7 |  |
| `webrtc_sky/webrtc_sky.cpp` | gone: motion::Timeline::apply | ch:: → motion:: ×1; Skia geometry types ×1 |
| `winamp_base/Equalizer.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia faces → weave::Face ×8; Spread/ramp/through → Tween/stagger ×5; Skia geometry types ×4; material::Paint in Compose ×3; ch:: → motion:: ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; shape protocol → Outline(glm::vec2) ×1; Skia colours ×1 |
| `winamp_base/Player.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia faces → weave::Face ×8; Spread/ramp/through → Tween/stagger ×5; Skia geometry types ×4; material::Paint in Compose ×3; ch:: → motion:: ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; shape protocol → Outline(glm::vec2) ×1; Skia colours ×1 |
| `winamp_base/Playlist.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia faces → weave::Face ×8; Spread/ramp/through → Tween/stagger ×5; Skia geometry types ×4; material::Paint in Compose ×3; ch:: → motion:: ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; shape protocol → Outline(glm::vec2) ×1; Skia colours ×1 |
| `winamp_base/Skin.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia faces → weave::Face ×8; Spread/ramp/through → Tween/stagger ×5; Skia geometry types ×4; material::Paint in Compose ×3; ch:: → motion:: ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; shape protocol → Outline(glm::vec2) ×1; Skia colours ×1 |
| `winamp_base/winamp_base.cpp` | **stops at** missing header <sigilgeometry/path/Skia.h> | Skia faces → weave::Face ×8; Spread/ramp/through → Tween/stagger ×5; Skia geometry types ×4; material::Paint in Compose ×3; ch:: → motion:: ×2; layer styles/marks → material effects ×1; textFx keys/Kinetic.h ×1; geometry door include ×1; shape protocol → Outline(glm::vec2) ×1; Skia colours ×1 |
| `world_hud/world_hud.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia geometry types ×10; material::Paint in Compose ×9; text Skia paint → ink(material) ×2; Spread/ramp/through → Tween/stagger ×2; layer styles/marks → material effects ×1; shape protocol → Outline(glm::vec2) ×1 |
| `y2k_chrome/y2k_chrome.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | material::Paint in Compose ×10; layer styles/marks → material effects ×9; Skia geometry types ×9; text Skia paint → ink(material) ×7; geometry door include ×3; engine/timeline ×2; Skia colours ×2; textFx keys/Kinetic.h ×1; Skia blend/tile/sampling ×1 |
| `yarn_marquee.cpp` | conversion: SkSize → glm::vec2; gone: material::skia::toSkColor; conversion: vec<2, float, (glm::qualifier)0U> → SkVector | Skia geometry types ×4; Skia colours ×2 |
| `zellige/zellige.cpp` | **stops at** missing header <sigilcompose/brush/LayerStyles.h> | Skia geometry types ×18; layer styles/marks → material effects ×2; material::Paint in Compose ×2; Skia colours ×1 |
| `p5_refractive_metaballs/p5_refractive_metaballs.cpp` | gone: sketch::Assets::shader | Assets::shader → hub ×3 |
| `xcom_battlescape/xcom_battlescape.cpp` | gone: sketch::Assets::json | Assets::json → hub ×1 |

## (d) The plates that may move, and why

Rebase these only, each with the cause named in the commit; every other plate stays byte-identical.

- **Motion timing:** `chladni_tab1`, `nightingale_coxcomb` (the old `ramp()` truncated to whole
  milliseconds); any sketch whose tween now starts from the value on screen; entrances by float
  rounding (the path is `interpolate(from, home, ease(t))`); a plate using `textFx::pop` (its
  reserved overshoot reach shrank from 0.17 to about 0.10 of nominal); a looping text track (the
  period is one beat plus `loopDelay`; under `.within` the beat includes the inner ladder).
- **Label and caption text:** specimen labels that print old names — `stock_materials`,
  `paint_shelf`, `field_shelf`, `blur_falloff`, `over_under`, `material_slots` — retitle to the new
  names ("label text"); `hub_reload` and `encode_write` print `load<ImageAsset>` in a caption
  ("caption text").
- **Feed readouts:** the kit connection readout's row label is "revision" (was "generation"), and a
  `state` row follows the door row: every sketch drawing `kit::connectionReadout` / a door moves
  (feed_sky and every instrument page showing a door).
- **Must NOT move:** the 11 replay sketches whose plates are THE check for `hub.replay` —
  artnet_lights, channel_bind, feed_events, feed_vitals, feed_sky, grpc_watch, midi_pads, osc_desk,
  phone_sky, serial_sensor, webrtc_sky — apart from the readout rows above.
- **Gradients:** none, provided a gradient placed in pixels says `{.units = GradientUnits::Pixels}`
  (box units are the default and measure from the box centre); a move here means a missed unit.
- **Hatches:** `stroke_atlas`'s crosshatch by an anti-aliasing step (the quarter turn is exact now).
- **Geometry:** extrusion caps are triangulated by CDT (earcut is gone) — every extruded solid's cap
  triangles (mesh_generators, scattered_model, any `mesh::extrude`); star outlines lose their explicit
  closing `lineTo` (fills identical, stroke joins the same); `shapes::Radial::points(size)` on an
  oblong box answers the vertices `radialOutline` draws in that box, where a unit ring stretched
  afterwards rounded apart by an ulp — a plate stamping marks at those vertices may move by a bit.
- **Media:** frame choice at a boundary for gif_frames and sticker_collection (ms floats → chrono
  doubles); a video leaf defaults to image-leaf layout (its size stated by the node); the image leaf
  `image(PixelSource, Fit)` defaults to `Fit::Contain` (CSS `object-fit: contain`) — a leaf that
  relied on the old default moves, and an atlas cell under `imageRegion` must spell `Fit::Native`.
- **Material model:** every sketch whose backdrop has grain or vignette (the sketch kit's grain is
  `material::noise(…, {.grain = true})` soft-lit, the vignette a box-unit radial;
  `sketch::kit::Backdrop` lost `over`); every former `layerStyle` site; y2k_chrome's aqua/gloss/
  chrome looks (rewritten onto Filter effects, not verbatim); a mark that sat in `.overlay()` (under
  the content) now painted in the material's effects slots (shadow beneath the fill; inner shadow,
  stroke and bevel over the fill AND the content) — a few pixels.
- **Rows kit:** gap/labelGap default 5/10 px — pass `theme().spacing.rowGap/labelGap` where the plate
  must not move; `Bars::rest` defaults to none — state the dimmed track where one was drawn (neither
  is a licensed mover).
- **Measure:** hitman_verlet keeps its hand peak if its plate must hold.
- **Shader:** none (same program text and uniforms; `.key` pins the name).

## (e) What the sketch pass must NOT do

- **No library edits.** Not a header, a source, a README, a test or a binding under `src/common/`,
  `src/sigilweave/`, `src/sketch/include`/`kit` library code, or `apps/python/sigil/`. If a sketch
  cannot say what it needs with the libraries as they stand, write the gap to
  `apps/spell-circle-canvas/FINDINGS.md` (re-read it first; state what the code does, what it was
  evidently meant to do, what a test should assert) and commit it by path; the owner decides.
- **A sketch never builds a Skia paint for text.** Text looks are said with `.ink(material)` or the
  cascade; Compose lowers the Material and hands Weave the paint (owner decision 30, 2026-09-28).
  Weave's paint model stays the renderer's paint by design, so no sketch reaches for
  `PaintStyle::foreground`, `PaintLayer::paint`, `PaintLayer(SkColor, …)`, `Decoration::paint` or
  `.paint.addUnderlay/addOverlay` directly; sites that do are rewritten onto `ink(material)`.
- No rebase of a plate outside (d); no rebase without its cause in the commit.
- No new flags, verbs or scripts for proofs or measurements; no sanitizers, formatters or ledgers.
- No `git add -A`/`.`/`-a`, `stash`, `reset`, `amend`, `rebase`, `checkout --`; no push.
- No re-introduction of a deleted look into a library: looks live in sketch-owned headers (see the
  Material table), bodies byte-identical.
- Not the pass's: the Kit fills that compare by pointer; the FINDINGS entries that name library
  defects (e.g. `Filter::of(material, SkColorType)` — use `skia::lowered`).

## (f) Verification (once, at the end)

1. `buildslot.py build SigilSketches Grimoire --who <name>` — both link.
2. `ctest --test-dir build -C Release --output-on-failure` through `buildslot.py run`. The reds
   that exist today only because Grimoire is stale turn green once it is rebuilt:
   `python_authoring`, `python_protocol_routes`, and the Grimoire host lanes
   `sketch_reload_runs_the_file`, `sketch_reload_materials`, `sketch_reload_surface`,
   `sketch_reload_surface_{video,set,device,web,substance}`, `sketch_reload_named`,
   `sketch_reload_directory`, `sketch_capture_viewport`, `sketch_window_orientation` (each refuses
   with "framework headers are newer than this host"). Known and not the pass's: the two Compose
   log-once cases in FINDINGS fail only when `compose_test` runs unfiltered in one process;
   `SketchLoadingClock.APageOpenedUnderIt…` needs a web engine.
3. The plate sweep: `python3 scripts/sigil.py plates` (cpu tier, byte identity against the
   machine-local baseline manifest); every mover in (d) rebased with
   `python3 scripts/sigil.py plates --sketch <name> --rebase` and its cause in the commit; every
   other difference is a FINDINGS entry, not a rebase.

