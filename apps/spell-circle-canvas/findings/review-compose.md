# Merge-readiness review — SigilCompose, SigilSkia, SigilScry

Branch `sigil/library-campaigns` → `main`, merge base `aabd3fe1b224`. Read-only.
All paths are under `/Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/`.
Scope: `src/common/compose` (with README.md, TYPOGRAPHY.md), `src/common/skia`,
`src/common/scry`. Every entry in the top ten and every blocker was re-read in the
current tree; the Skia release-proc ordering was checked against
`/Users/long/REI/skia-docs/skia/src/gpu/graphite/ImageFactories.cpp:82,597`.

Format: `severity | path:line | what the code does | what it should do | fix`.

## Ranked top ten

1. blocker | `src/common/skia/graphite/TextureImageMetal.mm:39` | `if (!image) CFRelease(retained);` after `SkImages::WrapTexture` — but Skia builds its `RefCntedCallback` before validating (`ImageFactories.cpp:82`) and every null return destroys it, running the proc, so a failed wrap releases the `MTLTexture` twice | the release proc is the only release once Skia has the context | delete line 39.
2. blocker | `src/common/skia/graphite/TextureImageMetal.mm:68` | `if (!image) return refuse();` after `TextureFromYUVATextures` — same ordering (`ImageFactories.cpp:597`), so the caller's `release(releaseContext)` runs twice on a refused planar wrap (decoder plane buffer freed twice) | only pre-call rejections may `refuse()` | change line 68 to `return image;`.
3. blocker | `src/common/compose/include/sigilcompose/kit/Grid.h:383-389` | flow search `for (;; ++at)` with `if (c + wide > cols) continue;` — a child whose `.cells(0,0,5,1)` span exceeds the column count and whose `.area()` name is unknown (`:354` leaves `declared=false`, `columns=5`) never exits: the layout pass hangs | a span wider than the grid is clamped | `const int wide = std::clamp(s.columns, 1, cols);`.
4. blocker | `src/common/compose/brush/Brushes.cpp:121-123, 252-255, 265-268` | three aggregate `PaintContext sub{ctx.size, path, ctx.elapsedSeconds, ctx.contentScale, ctx.animating, ctx.fonts, ctx.borrowed}` supply 7 of 10 members (`core/Paint.h:180-201`), so every brush nested in `Weave`/`Brush`/`Restyled` gets `stamps=nullptr`, `toRoot=I`, `rootSize=empty`: the stamp cache is defeated (re-rasterised every frame) and `worldSpace` materials anchor node-locally | copy the context and replace only the outline, as `Adaptors.cpp:11` and `Decorations.cpp:219` do | `PaintContext sub = ctx; sub.outline = paths[i];` at all three sites.
5. should-fix | `src/common/compose/typography/TextAnnotations.cpp:61-84` | `readings[index]` is paired with `units[index]`, but `unitsOfText` reports a base broken across a line as two entries; the head gets `shareOfReading` of its reading and the tail gets the NEXT reading, shifting every reading after it by one | both pieces share one reading (`Annotation.h:36-39`) | when `continues`, emit head share for `index`, tail share for `index+1`, then skip `index+1`'s reading.
6. should-fix | `src/common/compose/typography/TextFxPainting.cpp:301-304` | fx selection cache is stale on `contentRev`, `measuredForWidth`, selector list — not `measuredForHeight`, while layout keys on both (`core/Layout.cpp:111-113`); a vertical or depth-bounded leaf resized in depth reflows its lines under an old `sel::line(n)` mask, glyph count unchanged so the `:311-316` guard misses it | key on every measure layout keys on | add `selectionHeight` to the state and compare it.
7. should-fix | `src/common/skia/include/sigilskia/draw/Direct.h:43,54` + `src/common/compose/core/Instances.cpp:131` | `Promoted::source` is a bare `const SkImage*` compared by address; `Atlas::cell()` does `m_sheet.reset()` without clearing `gpuCache`, so a re-baked sheet at a reused address returns the OLD texture on Graphite (new cells never appear) | key on `SkImage::uniqueID()` (as `GraphiteContext.cpp:39` already does) | store `uint32_t sourceId` and compare `img->uniqueID()`; also `gpuCache = {}` beside the reset.
8. should-fix | `src/common/compose/core/Derive.cpp:239-246` | guard compares raw `nextMeasure` but stores the sanitised value (`isfinite && > 0 ? v : 0`); Yoga reports NaN for an unlaid-out next frame, so the guard is true every round and the chain re-lays out and reports `moved` until `kConvergeRounds` is exhausted | compare what is stored | sanitise into a local, compare and store that.
9. should-fix | `src/common/compose/core/Coverage.cpp:72,167` | `inside = pixels[x] >= covered` with `covered = clamp(lround(threshold*255),0,255)`; `.threshold(0.0f)` is legal (`ElementShape.cpp:50` clamps to [0,1]) and yields `covered==0`, so every transparent pixel is inside and the trace is the whole box | threshold 0 means "any ink" | `pixels[x] > 0 && pixels[x] >= covered`.
10. should-fix | `src/common/scry/engine/CMakeLists.txt:56` | `SUITES SlotFillingTest WebViewGpuTest LABELS gpu` becomes the gtest filter `SlotFillingTest.*` (`cmake/Sigil.cmake:224-226`), which cannot match the instantiated `SlotDoors/SlotFillingTest.…` names (`EngineGpuTest.mm:205`); those four cases fall into the unlabelled group and FAIL (not skip) on a device-less machine | the label selects the instantiated names | `SUITES SlotDoors/SlotFillingTest WebViewGpuTest`.

## 1. Correctness

### compose/core
- should-fix | `core/StackingPainter.cpp:2405-2409` | the one bake site with no `if (layer)` guard and no 16 Mpx area cap, unlike the four sibling tiers (`:1816, :1962, :2122, :2260`) | same guard and cap | add both.
- should-fix | `core/ReconcileHost.cpp:310` | `YGNodeMarkDirty(inst.yoga)` unguarded; `:259` and `Derive.cpp:204,248,438` all guard, and a text node inside `positioned()` has no Yoga node | guard | `if (inst.yoga)`.
- should-fix | `core/Instances.cpp:336` | `DataProps{atlas, pool, pool->revision(), blend}` — the memo key carries the pool's revision only; `Atlas::cell/variants/filter` after the first describe leave it equal and the node replays the picture recorded from the old sheet | the key carries the atlas's revision | give `Atlas` a `revision()` and add it to `DataProps`.
- should-fix | `core/Instances.cpp:66` | `Pool::resize()` grows `m_flights` with default `Flight{}` (from/to = origin), so the next `fly()` teleports appended instances to (0,0); `add()` (`:53-58`) and `flights()` (`:135-140`) fill at rest | fill new flights from the position lanes | copy `add()`'s fill.
- should-fix | `core/Composer.cpp:577-593` | `setBakeDensity` drops every bake and sets `paintDirty`, but `dirty()` (`:395`) reads only `contentDirty || needsLayout`, so a host gating `draw()` on `dirty()` never repaints | set the flag the query reads | `m_impl->contentDirty = true;` on change.
- should-fix | `core/Layout.cpp:451` | `minimumSizeOf` restores the probe only `if (wasWidth >= 0)`; `measuredForWidth` is `-1` by default and after a degraded layout (`:261-262`), so exactly then the child is left wrapped at nil width for the frame | restore unconditionally when `child.paragraph` | use the node's resolved box when `wasWidth < 0`.
- should-fix | `core/Layout.cpp:536-538` | `sizesWidth` infers "this scheme wrote the width" from `YGStyleGetWidth().unit == YGUnitPoint`, but `:506` (the scheme's own placement loop) writes point widths on children, so a `layout()` nested in a `layout()` overrides its parent's placement | track the write explicitly | a flag on the instance.
- should-fix | `core/Derive.cpp:302, 346, 360` | `ChainFill::cursor` is written and never read (`ComposerImpl.h:400`); frames after `last` keep the pre-balance fill and downstream runs start at the wrong word within a 3-round budget | thread the cursor into frames after `last` | consume `filled.cursor`.
- should-fix | `core/Derive.cpp:290-294` | `filled.overflowed` is the last non-skipped frame's verdict (`continue` on a degenerate box), which `holds()` (`:347`) bisects against | a run with a skipped tail does not hold | treat skipped tail as overflow.
- should-fix | `core/Factories.cpp:119` | `picture(pic, native)`'s custom key is `"picture:" + uniqueID` only; `propsEqual` compares only the key for keyed customs (`Reconcile.cpp:493-494`), so two describes differing in `native` with overridden dims prune together and keep a stale `recordedAt` | fold `native` into the key | append `native` dims to the key string.
- should-fix | `core/ElementLayout.cpp:199-202` | `tether()` appends `Bounds` reads for the new tether and fallbacks but never erases the previous tether's reads (`thread()` at `ElementText.cpp:230-240` does) | last-wins on reads too | `erase_if` the prior tether's reads first.
- should-fix | `core/Coverage.cpp:150-162` | the trace saves/restores five painter fields but not `recordingDeviceBakes`, `recordingDeviceDeferred`, `recordingMatrixStable`, which `PictureBake::replay` (`StackingPainter.cpp:61`) adds to; a replay inside a trace pins the enclosing recording | same scope struct `recordPicture` uses (`:111-114`) | reuse it.
- should-fix | `core/StackingPainter.cpp:2479-2484` + `core/BakeInk.h:176-179` | ink clip bakes `deviceClip` into the recorded region but the picture is remade only on matrix change (`:2536-2537`); a clip that grows under a fixed matrix replays the smaller region | key on the clip too | store `pictureDeviceClip` beside `pictureMatrix`.
- should-fix | `core/StackingPainter.cpp:534` + `core/Coverage.cpp:157` | the coverage trace recurses through full `paint()`, so profile rows, `nodesPainted`/`texturesBaked` and promotion `accrue()` count twice per traced frame (promotion warms at double rate under `Boundary::Coverage`) | no accrual under a trace | suppress while `coverageTrace != nullptr`.
- should-fix | `core/Derive.cpp:383-385` | comment claims one cached coverage answer, but derive traces at `hostScale` and the painter at `scale * node.bakeScale` (`StackingPainter.cpp:2395,2412`); `coverageOutline`'s key includes scale (`Coverage.cpp:107-110`), so a flowed-around baked node re-rasterises an A8 layer every frame | one scale both sides | trace at `hostScale` in the painter.
- should-fix | `core/Composer.cpp:528-533` | profile sort tie-breaks on `label`, but `profileLabel` is `"box WxH"` for keyless nodes (`StackingPainter.cpp:1137`), so equal rows are unordered under `std::sort` | deterministic | `std::stable_sort`.
- should-fix | `core/Diagnostics.cpp:51`, `core/Composer.cpp:369`, `core/Layout.cpp:584`, `core/ElementIdentity.cpp:31`, `core/ReconcileHost.cpp:111`, `core/ElementText.cpp:21`, `typography/TextMarks.cpp:64`, `typography/TextEffect.cpp:24,65,93`, `brush/StrokePainting.cpp:61` | once-warn sets/flags that are plain `static`, while `Diagnostics.cpp:95,107`, `TextFxPainting.cpp:176`, `Grid.h:169` use `static thread_local`; the tier has no locks, so two composers on two threads race on the hash sets | one spelling | `static thread_local` everywhere.
- should-fix | `include/sigilcompose/core/Paint.h:292` | `StampCache::put` clears the store when full BEFORE the replace scan, so re-baking one of 16 live stamps discards all 16 | clear only when inserting new | move the capacity check below the scan.
- should-fix | `include/sigilcompose/core/Pattern.h:154-159` | `bake()` installs a program into the shared `Tile` state capturing a raw `FontContext*`; `Tile::invalidate()`/`seed()` (`sigilmaterial/pattern/Tile.h:44-49`) re-run that program later, after the `FontContext` may be gone, and copies of the `Pattern` share the state | do not retain a borrowed pointer beyond the call | install per bake with the fonts passed, or capture an owning handle.
- nit | `core/Derive.cpp:331` | `chain[last]->description` dereferenced unchecked while `chain[first]` is checked at `:323-326` | one rule | guard or drop the other guard.
- nit | `core/Derive.cpp:128-140` | `hang(stated)` returns early on unknown/cyclic key, so the first RESOLVING fallback stands, contradicting the comment and `Derive.h:200-201` | match the doc | keep the stated one when none fits.
- nit | `core/Reconcile.cpp:768` | `painter->foldable(...)` after a null check guarding a different statement (`:707-709`) | guard the call | `painter && painter->foldable(...)`.
- nit | `core/Reconcile.cpp:981-985` | free rail waypoints are indexed under the empty key; `deriveRoute` skips them (`Derive.cpp:562-564`) but `routesAt("")` answers with them | skip | `if (anchor.nodeKey.empty()) continue;`.
- nit | `core/ElementLayout.cpp:184-188` | `cells()` clamps `columns`/`rows` but not `column`/`row`; negative origin vanishes the child (`Table.h:82-84`) | clamp both | `std::max(column, 0)`.
- nit | `core/StackingPainter.cpp:1148-1162` | `BakeLayerScope` says "a device of its own" but resets only the replay matrices; `deviceBlit` (`:1596-1605`) still bumps the outer `recordingDeviceBakes` | reset the counters too | extend the scope.
- nit | `core/StackingPainter.cpp:817/836/1052/1069` | `enterOwn()` save and `if (node.clipContent) canvas.save()` interleave, correct only because `hostsSpace()` refuses clipping nodes (`Depth.cpp:57`) | state the invariant | assert beside `enterOwn`.
- nit | `core/StackingPainter.cpp:313-319` | `forEachPlacedGlyph` walks every glyph to read the first glyph's font, per paint of any `textFill()` node | early exit | break after the first.

### compose/brush, draw, kit, texture, video
- should-fix | `include/sigilcompose/brush/Brushes.h:165-200, 231-309, 325-338`, `brush/Adaptors.h:31-52, 69-83` | `Weave`, `Brush`, `Restyled`, `EdgeSlice`, `Inset` forward `borrows/bleed/reach/isAnimated` but not `blends()`; `Volatility.cpp:404-405` reads the top-level decoration, so a wrapped `Overlay`/`Wash`/`Scanlines` is baked into a layer and blends against transparent black | forward it | `bool blends() const` OR-ing children (as `Layered.h:48-52` does).
- should-fix | `brush/SpanNormalForm.cpp:223-232` | `seamStraddled` tests `spans.front()`/`spans.back()` of the WHOLE path per contour; on a multi-contour path the last span may lie on another contour, so the seam stitch is skipped and the head is emitted as a separate subpath (the double-hit `:215-222` exists to prevent) | test the spans intersecting this contour | filter first.
- should-fix | `draw/Draw.cpp:73-74` | `pen.retained().get<Guest>(slot, [fonts]{...})` builds the `Composer` from the first frame's `FontContext&` and never rebuilds; a later pen with a different context leaves a dangling reference | rebuild when the context differs | store the pointer in `Guest` and compare.
- should-fix | `brush/Lines.cpp:38` | a second `insetOutline` (stroke-and-boolean) beside `geometry::path::insetOutline` (`sigilgeometry/path/Edges.h:58`, mitred offset); `Adaptors.cpp:18` calls geometry's, `Decorations.cpp:175` calls this one, so `inset(6, dec)` and `Border{.inset=6}` offset differently | one construction | delete `lines::insetOutline`, route both to geometry.
- nit | `brush/Brushes.cpp:483-484, 542` | authored `advance` is not floored; `Pattern{.advance = 1e-4f}` gives millions of tiles | floor at 1 px like the intrinsic branch | `std::max(..., 1.0f)`.
- nit | `brush/Brushes.cpp:288-304` | `len == 0` contour with fractional `interval` gives `step == 0`; a negative out-of-range `offset` makes `d < len` permanently true with zero increment | guard | `if (step <= 0 || len <= 0) continue;`.
- nit | `brush/Brushes.cpp:420, 456-462, 758` | stamp caches keyed on `bakedFor == art.node().get()`, a recyclable address | stable identity | `weak_ptr` or node id.
- nit | `include/sigilcompose/video/Video.h:59-60` | `image.width() / image.height()` with only the destination guarded (`:54`) | guard the source | early return on `image.isEmpty()`.
- nit | `include/sigilcompose/kit/Grid.h:290-291, 312-313, 342` | `place()` parses the area picture three times per layout (`flowed` twice, `readAreas` three times) | once | compute spans once and pass them to `solve`.

### skia, scry
- should-fix | `src/common/skia/include/sigilskia/draw/Direct.h:117` | `xDivs.empty() && yDivs.empty()` stretches to `dst`, but one-empty yields a single FIXED band (`DirectTest.cpp:89` pins `dst.back()==40` for `dstLen==100`) — the same lattice reads as stretch-all or stretch-none depending on the other axis | one rule for an empty axis | drop the early-out or make an empty list one stretchable band.
- should-fix | `src/common/skia/graphite/test/support/GraphiteReadback.h:62` | copies `width * 4` bytes per row regardless of the surface colour type; F16/F32 rows truncate | `info.minRowBytes()` | replace the width arithmetic.
- nit | `src/common/skia/graphite/test/support/GraphiteReadback.h:57` | `allocPixels` aborts the process on failure; the scry twin uses `tryAllocPixels` | fail the case | `tryAllocPixels`.
- nit | `src/common/scry/engine/test/EngineGpuTest.mm:58` | `EXPECT_NE(engine, nullptr); return *engine;` — null deref after a non-fatal expectation | abort the case | check and `std::abort()` or return a pointer.
- nit | `src/common/skia/graphite/test/GraphiteTest.mm:310` | `ctx` from `GraphiteContext::create(*dev)` used at `:316` without assertion; the skip macro checks a different context | assert the subject | `ASSERT_NE(ctx, nullptr)`.
- nit | `src/common/skia/graphite/OffscreenSurface.cpp:18` | defaulted move leaves `other.m_context` live, so a moved-from surface still `submit()`s (`QtInterop.h:24` returns by value) | moved-from does nothing | hand-write the move, null the context.

### typography
- should-fix | `typography/TextFxPainting.cpp:473` | glyphs dropped for alpha/scale ≤ ε return before the pass-lane routing at `:598-625`, so a fully-faded pass unit contributes no `keys` entry while `beatsOfTrack` (`TextMarks.cpp:322-345`) still counts the beat; `uUnitRect[i]`/`uUnitPhase[i]` renumber against the query (`TextEffect.h:507-509`) | same enumeration both sides | route before the drop.
- should-fix | `include/sigilcompose/typography/TextEffect.h:264-283` | `operator==` omits `reach`; `fx::effect(key, program, reach, params)` passes it to the constructor only, so widening the reach prunes into the old body and truncates cached output (`Track.h:91-92`) | compare it | add `reach` to the comparison or append it to `params`.
- nit | `typography/TextPainter.cpp:154-156` | drive identity is `"variationDrive:%.4s@%p"` and the lambda stores the raw `Output*` for the description's lifetime; a destroyed-then-reallocated Output prunes onto the dead drive | stable identity | counter or binding identity.
- nit | `typography/PathText.cpp:252-259` | `Align::Center/End` with small `at` gives negative `entryLocal`; the loop only advances forward, and `:271-274` then overstates the interval on an open baseline | clamp | clamp to `[0, contourLength]`.
- nit | `typography/TextMarks.cpp:293` | `if (!inst.paragraph.has_value()) return {};` is dead — `resolveTrackSchedule` returns false at `:254` | delete.
- nit | `typography/TextMarks.cpp:252` | `resolveTrackSchedule(Composer::Impl& impl, …)` never reads `impl`, threaded through two callers | drop the parameter.

## 2. Public API

- should-fix | `src/common/skia/include/sigilskia/graphite/TextureImage.h:59, 81` | two `wrapImage` overloads with opposite ownership: single-plane "THE IMAGE HOLDS THE TEXTURE" (`:47`), planar "NOTHING IS RETAINED HERE" (`:71`) plus a release protocol the other lacks | ownership in the name or the signature | `wrapPlanarImage`, or give both the release pair.
- should-fix | `include/sigilcompose/brush/Brushes.h:409, 526, 701` vs `:180, 276, 332` | `Scatter`, `Pattern`, `Art` expose `float reach` as a FIELD documented as a cull reserve, so they fail `ReachingDecoration` (`Shape.h:399-402`) and fall to the bleed branch; `Shape.h:387-398` says reach and bleed are different numbers | one shape | rename the fields `bleed` and add `reach()` methods.
- should-fix | `include/sigilcompose/kit/Kinetic.h:278, 296` | two `marquee` overloads differing by an inserted positional float — `marquee(e, 100.f, 0.f)` reads as (phase, gap) or (contentWidth, phase) | one options struct | `marquee(Element, MarqueeOptions)`.
- should-fix | `include/sigilcompose/kit/Layouts.h:71` | "Participates in equality like every field" — `Radial`, `AlongPath`, `ModularGrid`, `Diagonal`, `BaselineGrid`, `Scatter` declare no `operator==` and `layout()` erases into a `std::function` with no comparator (`Factories.h:184-186`) | say what is true | delete the sentence or add defaulted `==` and a comparator through `makeLayout`.
- should-fix | `include/sigilcompose/typography/TextEffect.h:223, 554` | effect factories in two homes: `TextEffect::variableAxis`/`TextEffect::pass` as statics and `fx::pass/scramble/keys/seq/mix/hold/effect` as free functions; TYPOGRAPHY.md:458 shows the seam | one namespace | `fx::variableAxis` forwarder (as `fx::pass` already forwards).
- should-fix | `include/sigilcompose/core/Composer.h:457-469` | two stacked `/** */` blocks with no declaration between; the first documents a setter but attaches to the `PromotionPolicy` enum and its second paragraph starts mid-sentence ("yourself with") | one block on the setter | merge and move.
- should-fix | `include/sigilcompose/core/TextPainter.h:85` | `std::string_view frameKey` built from `inst.description->key` (`Instance.h:730-736`) and stored by value in `GlyphStructure` (`TextEngine.h:74`) inside `static thread_local` objects; the description is replaced on every patch | own it | `std::string`.
- should-fix | `include/sigilcompose/core/Derive.h:82-147, 248-309` + `Shape.h:113` | `Router` and `RailRouter` are the same erased-comparable class twice (identical `State`, `comparable()`, `operator==`), and `sigilcore/comparable/Erased.h:32-91` already owns the mechanism | one template | build both on `Erased`.
- should-fix | `include/sigilcompose/core/Table.h:41` | the only example writes `layouts::Table{…}`; the struct is `sigil::compose::Table` (`:53`) and `layouts::Table` exists nowhere | compiling example | drop the qualifier.
- should-fix | `include/sigilcompose/core/Paint.h:87, 95` | `hexColor` and `hex` both public, `hex` forwarding to `hexColor` | one name | keep one.
- nit | `include/sigilcompose/typography/Track.h:75` vs `typography/Annotation.h:50` | granularity is `Track::over`/`innerOver` on one value and `Annotation::unit` on the other | one word | rename one.
- nit | `typography/TextPose.h:23` | `using namespace detail;` at namespace scope in a header included by four TUs | qualify | drop the directive.
- nit | `include/sigilcompose/core/Element.h:168, 182` | `cells(int,int,int,int)` where `CellSpan` (`Layout.h:171`) exists; `area(std::string)` by value where `thread`/`key` take `string_view` | one shape | `cells(CellSpan)`, `area(string_view)`.
- nit | `include/sigilcompose/core/Instances.h:296-311` | `kCullThreshold`, `stamp()`, `DataProps` exported in the consumer header | private header | move beside `Instances.cpp`.
- nit | `include/sigilcompose/core/Instances.h:339` | `std::optional` used with no `<optional>` include | include it.
- nit | `include/sigilcompose/core/TextPainter.h:36-37, 114` | `struct SkSize;` forward-declared then taken by value | include the definition.
- nit | `include/sigilcompose/core/Derive.h:170-179` | `Anchor` reads `point` or `norm` depending on `nodeKey.empty()`, stated only in prose | a variant or a named discriminator.
- nit | `core/Composer.cpp:708, 734, 766, 774` | `const_cast` on `*m_impl`, already non-const through `unique_ptr` | delete the casts.
- nit | `core/Volatility.cpp:383` / `core/Transitions.cpp:334` / `core/Reconcile.cpp:330` | pan-only spelt `boundOffsetLive()` / `hasBoundOffset()` ×2 | one helper.
- nit | `include/sigilcompose/kit/Grid.h:95` vs `kit/Placers.h:52`; `kit/Layouts.h:286` vs `brush/Brushes.h:397` | `repeat` and `Scatter` each name two unrelated things in the kit | distinct names | `repeatTrack`, `Jittered`.
- nit | `include/sigilcompose/kit/Chrome.h:31` vs `kit/Gel.h:62`, `kit/Gloss.h:48` | era options mix `material::Color` and `SkColor4f`, forcing conversions in `Eras.cpp` | one colour type.
- nit | `kit/Eras.cpp:93-94` | six positional floats into the six-float `AquaGloss` aggregate; `topFrac`/`alphaBottom` hardcoded and unreachable from `AquaGelOptions` | designated initialisers; expose the knobs.
- nit | `include/sigilcompose/kit/Specimen.h:180` | printf-style variadic `formatted(const char*, Args...)` with no format attribute or type constraint | add the attribute and a trivially-copyable requirement.
- nit | `include/sigilcompose/kit/Kinetic.h:78, 91, 122, 177` | `rise`, `slide`, `spinIn`, `scatter` are one `(1 - ease(t)) × offset` with different lanes and unexplained alpha constants (`t/0.35`, `t*1.7`, `t*2.2`) — enumerated variants of one shape | one general entrance with props | `fx::enter({.dx,.dy,.rotateDeg,.fromScale,.curve})`, presets over it.
- nit | `include/sigilcompose/kit/Kinetic.h:62, 266` | two namespaces (`fx::` presets and `kit::` marquee) in one header | split | `kit/Marquee.h`.
- nit | `src/common/skia/include/sigilskia/graphite/OffscreenSurface.h:52` vs `TextureImage.h:47` | surface wrap borrows the `MTLTexture`, image wrap retains; both take bare `void*` | state it once | a sentence in `Skia.h`.
- nit | `src/common/skia/include/sigilskia/graphite/Pixels.h:26` | `halfFloatPixels` doc omits "premultiplied" though `Pixels.cpp:23` forces it | say it.
- nit | `src/common/skia/include/sigilskia/draw/Direct.h:156` | `drawSpriteAtlas` takes four parallel raw arrays and one `count` | a span struct | `SpriteBatch`.

## 3. README / TYPOGRAPHY drift

The probe guard (`test/docs/api_doc_probes.py:83-88`, 198 using-probes, 0 exclusions) compiles QUALIFIED `A::B` names and designated initialisers only; bare backticked names, paths, and prose are not checked. Everything below sits in that blind spot.

- should-fix | `README.md:437` | names colour spellings `hex`, `alpha`, `mul`, `lift`, `mix`; `core/Paint.h` has `hexColor`, `hex`, `alpha`, `scaleRgb`, `lighten`, `mix` — `mul` and `lift` exist nowhere | match the header | `scaleRgb`, `lighten`, add `hexColor`; and either widen the guard to bare names in the header-listing bullets or stop claiming "every API name".
- should-fix | `README.md:1397` | `python3 scripts/setup.py --config Release` — no such file; the verb is `scripts/sigil.py setup` | fix the command.
- should-fix | `README.md:1133` | `scripts/plate_ledger.py --tier promotion` — no such file; `sigil.py plates --tier promotion` | fix.
- should-fix | `README.md:1464` | `scripts/bench_ledger.py` — no such file; `sigil.py bench` | fix.
- should-fix | `README.md:39-48` (uses at `:71, :80, :103-105, :126-131`) | the opening example's prologue has only `using namespace sigil::compose` + chrono literals, then calls `animate`, `to`, `Transition`, `bind` unqualified and spells `weave::`/`motion::` — all live in `sigil::motion`/`sigil::weave`, and compose re-exports nothing (`:1269`) | a prologue that compiles | add `#include <sigilmotion/Animation.h>`, `namespace weave = sigil::weave; namespace motion = sigil::motion; using namespace sigil::motion;`.
- should-fix | `README.md:551-554` | says `motion::isLive` comes from `<sigilmotion/Animation.h>`; it is declared only in `sigilmotion/values/Animated.h:110`, which `Animation.h` does not reach | name the right header.
- should-fix | `README.md:422-424` | "The public include root is `include/` and nothing else" — `testing/CMakeLists.txt:11-12` adds `testing/include`, and `:850` cites `testing/Index.h` | state both roots.
- should-fix | `README.md:425-428` | "each feature has an umbrella over its public headers" — `core/Core.h` omits `core/Feed.h` and `core/Pattern.h`; `kit/Kit.h` omits five same-tier headers (`Grid`, `Ground`, `Layouts`, `Placers`, `Routers`) while `:956-957` only excuses other tiers' headers | umbrellas complete, or the carve-outs stated.
- should-fix | `README.md:1390-1394` | "nothing here links `SigilCompose`" — `src/sketch/sketches/CMakeLists.txt:150` and `src/sketch/set/CMakeLists.txt:9` link it; `compose/CMakeLists.txt:87` says so | say the sketch library links it.
- should-fix | `README.md:113` | shows the host gate `moving || composer.dirty() || ticker.active()`; the branch added `Composer::active()` (`Composer.h:179`) as the gate and it is absent from the canon | document `active()`.
- should-fix | `include/sigilcompose/typography/TextUnit.h:38-41` | "rect in the node's own space" — `Composer::units` lifts every rect into composer space (`Composer.cpp:743-757`; TYPOGRAPHY.md:929-931; `Track.h:161` says composer space for `Beat`) | composer space, same for `axis` at `:45-48`.
- should-fix | `include/sigilcompose/typography/TextEffect.h:310, 470, 475` | spells the pass requirement `Material::recipe`, which exists under no namespace; TYPOGRAPHY.md:610-613 has `material::skia::Paint::recipe` | fix all three.
- should-fix | `TYPOGRAPHY.md` (415-431) | documents `GlyphMod` but never `GlyphInfo` (`TextEffect.h:46-76`), the body's input; also absent: `GlyphModFn` (`:165`), `fx::kNominalSizePx` (`:440`), `Beats` (`Track.h:40`), `Track::reachPx` (`:107`), `TextEffect::composite/passMaterial/restPhases` (`:292, :318, :336`) | name them.
- should-fix | `include/sigilcompose/kit/Kit.h:41` | component table lists `format()` for `kit/Specimen.h`; the function is `formatted()` (`Specimen.h:174,180`, which explains why it is NOT `format`) | fix the table.
- should-fix | `include/sigilcompose/core/Measure.h:5` | `@file` names `measure`; the function is `intrinsicSize` (`:189`) | fix.
- should-fix | `include/sigilcompose/core/Element.h:322` and `core/StackingPainter.cpp:290-291` | "See `<sigilcompose/Material.h>`" — deleted on this branch | name `<sigilmaterial/skia/Paint.h>` or drop.
- should-fix | `include/sigilcompose/core/Mask.h:226, 233, 238, 254, 257, 268, 283, 294` | eight comments name `compose::Material`, deleted; `:283`'s "held out of line because Material is declared in its own header" is false (Mask.h includes `sigilmaterial/skia/Paint.h` directly) | update.
- should-fix | `core/CMakeLists.txt:6, 13-15` | lists "the material value" (deleted) and omits `SigilMaterial`/`SigilMeasure` from the PUBLIC closure it enumerates (actual list `:59-61`) | match the target.
- should-fix | `src/common/skia/include/sigilskia/Skia.h:4` and `src/common/skia/CMakeLists.txt:15` | "Every Qt-free SigilSkia header" — `draw/Direct.h` is Qt-free and not included; README.md:22-23 says graphite only | say "graphite".
- nit | `README.md` (whole) | `kit/Frame.h`, `kit/PixelType.h`, `kit/Sprites.h` are never spelled though their contents are (`:891-897`) | add the paths.
- nit | `README.md:1253-1254` | private link list omits `Boost::container`/`Boost::unordered` (`core/CMakeLists.txt:63`); brush tier omits `SigilComposeTypography` (`brush/CMakeLists.txt:41-42`) | complete the lists.
- nit | `README.md:1370-1372` | build section ships two kit headers with brush; `brush/CMakeLists.txt:39` ships four (`Flourish`, `Ornament`, `Plate`, `Strokes`) | list four.
- nit | `README.md:1404-1407` | example suite `ComposeKinetic` covers `kit/Kinetic.h` but sits in `typography/test/ComposeTestContentText.cpp:196` — contradicting the rule it illustrates | pick a conforming example.
- nit | `typography/TextMarks.cpp:205` | `entry.pitch = layout.linePitch` (layout-wide) while `TextUnit.h:49-51` calls it the line's pitch | reword.
- nit | `include/sigilcompose/core/Shape.h:3-11`, `core/Layout.h:3-8` | `@file` blocks omit `HeldPath`, `KeyedShape`, `Boundary` / `CellSpan`, `cachePolicy` added on the branch | update.
- nit | `src/common/skia/README.md:338` | names `SkiaPixels` but not the draw feature's `LatticeEdges` suite (`DirectTest.cpp:32`) while every other suite is named | add it.
- nit | `src/common/scry/README.md:251` | `cpuEngine()`/`gpuEngine()` presented as shared helpers; each is file-local to one binary | reword.

## 4. Comment rules

- should-fix | `core/BakeInk.h:52`, `core/StackingPainter.cpp:829, 2418, 2466` | history: "what the blit did before this existed", "the deleted trim() revealed", "this tier had before the grid existed", "as it was before the grid existed" | state the constraint | rewrite in the present.
- should-fix | `core/StackingPainter.cpp:289-291` | a measurement ("a `linearUnit` ramp came out at t ≈ 0.003 and every glyph painted the first stop") plus a citation to the deleted `Material.h` | keep the constraint (the ramp is in unit space) | cut the number and the file.
- should-fix | `core/StackingPainter.cpp:2550-2551` | performance claim ("Two clock reads … the overhead is not close to material") | cut.
- should-fix | `core/Reconcile.cpp:895-896` | "see the note there" pointing at `patchChildren()`, now SigilCore's with no such note; the rule lives in `ReconcileHost.cpp:355-361` | state the rule.
- should-fix | `typography/TextEngine.h:147` | "see the FIELD PINS block above" — no such block in the file | state the rule.
- should-fix | `include/sigilcompose/core/Element.h:912` | "SigilWeave's README is the canon for what each one means" — a document citation in a public header | state what a `ParagraphStyle` carries.
- should-fix | `include/sigilcompose/web/Web.h:4` | "(stress item 19)" — a worklist reference | delete.
- should-fix | `brush/Lines.cpp:33` | "NOT kFill — see above" with nothing above (orphaned by the split) | state why (a fill rec returns the solid path).
- should-fix | `src/common/scry/cmake/FindUltralight.cmake:16` | "See src/common/scry/README.md for SDK installation instructions" | delete the sentence (lines 8-14 already state the constraint).
- should-fix | include trailers naming symbols the file does not contain — `core/Effects.cpp:12,19,20`, `core/Diagnostics.cpp:14,21,22`, `brush/Masks.cpp:15,21,22`, `brush/SpanNormalForm.cpp:14,21,22`, `brush/StrokePainting.cpp:27`, `include/sigilcompose/brush/GeometryOps.h:13,14`, `typography/TextMarks.cpp:25`, `typography/PathText.cpp:26` | e.g. `// SkDebugf — the slot-rename diagnostic` on files with no `SkDebugf`; `// std::snprintf — variationDrive's effect key` with neither | delete with the includes (category 5).
- nit | `src/common/skia/include/sigilskia/draw/Direct.h:151` | "Reeves' 1982" — a date | drop the attribution.
- nit | `include/sigilcompose/brush/PixelStyles.h:38` | "from 1995 to 2005" | "the beveled-desktop era".
- nit | `include/sigilcompose/brush/Lines.h:12` | "Extension-point note:" — a marker other than `workaround:` | drop the label.
- nit | `include/sigilcompose/kit/PixelType.h:104, 231, 267, 346, 403` | "See trap 1", "trap 4" — section-number citations into the `@file` list | restate each constraint inline.
- nit | `brush/SpanNormalForm.cpp:252` | "(Bounds.cpp's trim block: …)" — file citation | keep the arithmetic, drop the file.
- nit | `typography/TextAnnotations.cpp:5` | "(layout/Beside.h)" citation | drop.
- nit | `typography/TextFxPainting.cpp:518` | "where the baseline no longer does" — history phrasing | "does not".
- nit | `core/Reconcile.cpp:283` | assertion string "(Mask::operator== is in Compose.h)" — it is at `Mask.h:320` | fix the string.
- nit | `core/ComposerImpl.h:345-364` | doc block for a `@p movingAbove` that no longer exists (now `Above above`, `:365`) | merge.
- nit | `core/ComposerImpl.h:566-569` | `concatTo`'s "THE SAME LIST" claim — four depth lanes are only in `matrix44()` | fix.
- nit | `core/ComposerImpl.h:394-409, 740` | six declarations filed under the wrong `.cpp` heading (defined in `Derive.cpp`) | move under a Derive heading.
- nit | `core/ComposeInternal.h:628` | FIELD PINS block names `Material` and `Effect`, now `material::skia`'s | update.
- nit | `src/common/scry/include/sigilscry/engine/WebEngine.h:117` | "exactly like an sigil::image::ImageAsset frame" — cross-library citation, ungrammatical | end at "raster or Graphite-backed".
- nit | `README.md:241, 834` | intra-document "see 3D, the CSS way", "see Boundaries" | inline the clause.

## 5. Leftovers

- should-fix | `core/Effects.cpp:6-23, 28` | 14 unused includes, an unused `using detail::Kind;`, and the file holds only `Fill::shader`, `Element::Element`, `NodeHandle::operator->` — no effects | strip and rename | `ElementNode.cpp`, two includes.
- should-fix | unused include blocks copied across the split — `core/Volatility.cpp:7-37` (~19), `core/StackingPainter.cpp:10,15,20,23,37,42,43` (7, and `<cstdio>` MISSING for `std::snprintf` at `:1137`), `core/Diagnostics.cpp:9-19` (9), `core/Transitions.cpp:10-12` (3), `brush/Masks.cpp:9-22` (12), `brush/SpanNormalForm.cpp:9-22` (9), `brush/StrokePainting.cpp:9-33` (~21), `include/sigilcompose/brush/GeometryOps.h:9-19` (8), `typography/TextMarks.cpp:7-40` (22), `typography/PathText.cpp:8-40` (20), `typography/TextFxPainting.cpp:9-42` (14) | each verified by grep: every named symbol occurs once, on its include line | prune.
- should-fix | `include/sigilcompose/core/Paint.h:23-29, 40` | seven includes a public header does not use (`sigilmaterial/core/Material.h`, `UniformBlock.h`, `sigilmaterial/skia/Effect.h`, `sigilmotion/Animation.h`, `schedule/Schedule.h`, `values/Animated.h`, `<string_view>`) | prune.
- should-fix | `include/sigilcompose/brush/GeometryOps.h:59, 65` + `brush/Brushes.cpp:50-63` | `ops::debug` (two `SkDebugf` loops) and `ops::chain` have no caller in `src/`; every `ops::chain` call site is `geometry::path::ops::chain` | delete both.
- should-fix | `core/Volatility.cpp:423` | `const bool liveEffect = …` never read | delete.
- should-fix | `texture/CMakeLists.txt:20-21` + `texture/Texture.cpp:141-148` | `SIGILCOMPOSE_TEXTURE_DEVICE=1` is now unconditional, so six `#ifdef/#else` pairs have a dead half, including a comment describing a build that cannot exist | delete the macro and the `#else` halves.
- should-fix | `core/StackingPainter.cpp:1541-1562` | `COMPOSE_PROF` env var read by `getenv` in the paint path, printing via `SkDebugf`, documented nowhere in the tree (`grep -r COMPOSE_PROF *.md *.py` is empty) | document beside `--bench`, or remove | one line in README's profiling paragraph or delete.
- nit | `include/sigilcompose/kit/Gel.h:102` (`aquaOrb`), `kit/Grid.h:95` (`layouts::repeat`), `include/sigilcompose/brush/GeometryOps.h:92-95` (`GeometryOp::operator==`) | public entries with zero callers | add a case or delete.
- nit | `core/ComposerImpl.h:400` | `ChainFill::cursor` written, never read (category 1 finding needs it) | consume or delete.
- nit | `core/Volatility.cpp:669` | `SkDebugf` with no `SkTypes.h` include | include.
- nit | `core/Diagnostics.cpp:57` vs `:98, :109`, `include/sigilcompose/kit/Grid.h:172` | diagnostics on two channels (`SkDebugf` / `fprintf(stderr)`), and kit prints from an inline public header | one channel | route through `detail::warn*`.
- nit | `include/sigilcompose/core/Shape.h:16-18` | two unused motion includes; the one motion symbol (`Animatable<float>`) is reached transitively | include `values/Animatable.h` only.
- nit | `src/common/scry/cmake/FindUltralight.cmake:36` | searches `/usr/local` only; the repo rule and `FindSubstance.cmake:28` look under `~/.local/opt/<name>/<version>/` | add that prefix and update README steps 285-294.

## 6. Duplicated mechanisms

- should-fix | `src/common/scry/test/GraphiteReadback.h:50` vs `src/common/skia/graphite/test/support/GraphiteReadback.h:29` | the snap-insert-read-submit-spin readback written twice with three divergences (lock, N32 forcing, alloc failure, spin bound), while `skia/graphite/CMakeLists.txt:46-51` publishes `SIGIL_SKIA_GRAPHITE_TEST_SUPPORT_DIR` for this consumer and scry's test targets do not use it | one copy | keep `sharedDevice()/sharedGraphite()` in scry, use the shared readback via `SUPPORT_DIRS`.
- should-fix | `include/sigilcompose/brush/GeometryOps.h:52` + `brush/Brushes.cpp:65` | `PathOp` alias and `chain` identical to `geometry::path::ops::PathOp/chain` (`sigilgeometry/path/Ops.h:225,228`) | alias or delete.
- should-fix | `brush/Brushes.cpp:629-738` | `Ribbon::band` re-implements `geometry::path::bandRegion` (`sigilgeometry/path/Band.h:41-44` names this consumer); `brush/Band.cpp` already delegates | call `bandRegion`.
- should-fix | `core/Transforms.h:37` + `ComposerImpl.h:524,553,554,579,580`, `Query.cpp:104,105`, `LayerStyles.cpp:61`, `Decorations.cpp:130`, `Brushes.cpp:388,650`, `Lines.cpp:429`, `kit/Routers.cpp:252,254`, `kit/Kinetic.h:165`, `kit/Ornament.h:409` | degree↔radian constant/literal respelled ~16 ways; `geometry::path::kDegToRad/radians()` (`sigilgeometry/path/Numeric.h:21-34`) is the owner and already used by `kit/Layouts.h:90` | one spelling.
- should-fix | `include/sigilcompose/core/Table.h:176-207` vs `include/sigilcompose/kit/Grid.h:341-400` | two cell-flow algorithms (`taken`/`claim`); Grid's is the general one | core owns one flow the kit configures.
- should-fix | `include/sigilcompose/core/Derive.h:82-147, 248-309` vs `sigilcore/comparable/Erased.h:32-91` | comparable type erasure copied twice into compose (tested only in core) | build on `Erased`.
- nit | `include/sigilcompose/core/Paint.h:87-140` | `hexColor/hex/alpha/scaleRgb/lighten/mix` over `SkColor4f` beside `material::rgb` and `sigilmaterial/skia/Color.h`'s linear-light mixing; `mix` here is component-wise in whatever space it gets | one colour vocabulary.
- nit | `brush/Brushes.cpp:600-608` | `turnedArea` is `Polyline::signedArea` (`sigilgeometry/path/Polyline.h:40`) | call it.
- nit | `include/sigilcompose/kit/Ornament.h:404` | `starburstOutline(s, depth)` is `geometry::shapes::star(s, 1 - depth)` (`sigilgeometry/kit/Generators.h:86`) | forward.
- nit | `include/sigilcompose/kit/Ornament.h:82-89` | hand-rolled cubic Bernstein at fixed 22 steps; `geometry::path::flatten` (`Polyline.h:72`) is tolerance-adaptive | use `flatten`.
- nit | `brush/Lines.cpp:380` vs `sigilgeometry/kit/Hatches.h:50` | two `Hatch` constructions, units differ (`angle` rad vs `angleDeg`) | grow geometry's with `cross`, compose holds bindings.
- nit | `typography/TextFxPainting.cpp:84-89` vs `typography/PathText.cpp:93-98` | `axisLadderSteps`/`tangentLadderSteps` same body, different constants | one `ladderSteps(px, perPixel, min, max)`.
- nit | `typography/TextFxPainting.cpp:613-618`, `TextMarks.cpp:188-197, 333-337` | the reverse-scan "find key else append" written three times | one helper.
- nit | `src/common/scry/engine/test/Wait.h:43` | poll-until-deadline helper local to one library's tests | grow shared test support if a second consumer appears.

## 7. Files over ~600 lines

- should-fix | `core/StackingPainter.cpp` (2582; `Composer::Impl::paint` alone is `:1200-2580`, `paintContent` `:349-1091`) | one 1380-line function | split the cache ladder (promotion, split bake, `Cache::Group`, device/local texture bakes, `BakeLayerScope`, `ProfileScope`) into `BakeLadder.cpp`, leaving `paintContent` and silhouette helpers.
- nit | `include/sigilcompose/core/Element.h` (1303) | the `.cpp` side was split into eight `Element*.cpp` on this branch, the header was not | split by the existing `// ---- section ----` banners (layout, shape, mask, paint, decoration, depth, content, span restyling).
- nit | `core/Reconcile.cpp` (1044) | structural comparators (`:96-620`) → `ReconcileEquality.cpp`.
- nit | `typography/TextFx.cpp` (979) | glyph structure / selection / cascade / `fx::` combinators → `GlyphStructure.cpp`, `Selection.cpp`, `Cascade.cpp`, `Combinators.cpp`.
- nit | `brush/Brushes.cpp` (847) + `include/sigilcompose/brush/Brushes.h` (735) | composites / stamps / ribbon / art → `Composites`, `Stamps`, `Ribbon`, `ArtBrush`, `Brushes.h` as umbrella.
- nit | `core/Composer.cpp` (801) | run helpers `metrics/atCapHeight/measureRun/runPens/fitRun` (`:121-265`) → `TextMeasure.cpp`.
- nit | `core/ComposerImpl.h` (785) | `NodeTransform` + matrix builders → `NodeTransform.h`; `Space/HitSpace/depthMatrixOf` (`:600-650`) → `Depth.h`.
- nit | `core/Volatility.cpp` (757) | world-matrix scan (`:136-228`) → `WorldMatrix.cpp`.
- nit | `core/Instance.h` (738) | caching block (`:279-510`) → `InstanceCaches.h`.
- nit | `typography/TextFxPainting.cpp` (733) | substitution gates (`:62-188`) → `Substitution.cpp`; `bandOf/glyphBox` (`:190-256`) → `GlyphBox.cpp`.
- nit | `core/Layout.cpp` (710) | `layoutText`/`minimumSizeOf` → `LayoutText.cpp`.
- nit | `include/sigilcompose/typography/TextEffect.h` (694) | value type vs `fx::` catalogue → `TextEffect.h`, `Fx.h`.
- nit | `core/ComposeInternal.h` (694) | description structs apart from shared helpers.
- nit | `core/Derive.cpp` (629) | frame-chain family (`:168-370`) → `Threads.cpp`.
- nit | `include/sigilcompose/kit/Grid.h` (605) | ~300 lines of private `resolve/flowed/share` bodies inline in a public header → `kit/Grid.cpp`.
- nit | test files `core/test/ComposeTestKernel.cpp` (4164), `typography/test/ComposeTestContentText.cpp` (3816), `brush/test/ComposeTestStrokeGrammar.cpp` (2373), `ComposeTestLines.cpp` (2269), `ComposeTestBrushes.cpp` (1681) | the CMake comment promises "one file per subject, named for what it asserts" | split by suite.

## 8. Test gaps

- should-fix | `src/common/skia/include/sigilskia/graphite/TextureImage.h:81` | planar `wrapImage` has no case anywhere (`TexturePlane`/`SkYUVAInfo` appear only in decl, impl, README, `video/decode/DeviceMetal.mm:145`) — the path with blocker #2 | wrap + each refusal asserting the release ran exactly once.
- should-fix | `src/common/skia/include/sigilskia/draw/Direct.h:111, 156` | `drawLattice`, `drawSpriteAtlas`, `ready`, `Promoted` have no case in `draw/test/` (only `latticeEdges`) | chunk boundary (16000/16001), non-uniform `sizes`, `ready()` identity.
- should-fix | `core/Coverage.cpp:72` | `.threshold(0.0f)` never passed — `ComposeTestMask.cpp:327` skips exactly that value | a case asserting the silhouette, not the box.
- should-fix | `typography/TextAnnotations.cpp:74-84` | no `shareOfReading` case in `typography/test/`; the two `ComposeAnnotate` cases assert only `reserve` | a two-reading annotation whose second base wraps.
- should-fix | `core/Depth.cpp:57-67` | `hostsSpace`'s eight refusals untested (`ComposeTestDepth.cpp` has no `clip(`, `opacity(`, `blend(`, `Cache::`, `boundary(`) | one case per refusal.
- should-fix | `core/Volatility.cpp:347-349, 729-733` | projection/space volatility branch and the deferred-effect tier (`effectOnly`/`deferLayerEffect`) have no test | a static plane under animated `perspective()`; a deferred effect re-record.
- should-fix | `core/Instances.cpp:60-70, 128` | no case draws, registers a cell, draws again; none `resize()`s upward with a flight lane | both.
- should-fix | `include/sigilcompose/core/Composer.h:179` | `Composer::active()` untested | a settled external binding written between two draws.
- should-fix | `include/sigilcompose/kit/Chrome.h`, `kit/Gel.h:61,78,102`, `kit/Typeset.h:332,358` | `ChromeOptions/ChromeBody/ChromeSliver`, `AquaBody/AquaGloss/aquaOrb` (incl. the `bleed()` contract), `BlockRule`/`kit::rules` (three `Where` arms) constructed in no test | `kit/test/ComposeTestEras.cpp`; one case per `Where`.
- nit | `src/common/skia/include/sigilskia/graphite/GraphiteContext.h:78,83,103,107,122` | `makeRecorder`, `lockContext`, `makeRecorderOptions`, `makeContextOptions`, `reportShaderErrorsTo` untested; `SIGILSKIA_GLYPH_ATLAS_BYTES` parsing (`GraphiteContext.cpp:94-96`) unexercised | device-free case over the options.
- nit | `src/common/scry/include/sigilscry/engine/WebView.h:166` | `scroll`'s documented sign convention has no case | scroll a tall page, read below the fold.
- nit | `include/sigilcompose/typography/TextPath.h:79-81, 89-97`, `Annotation.h:64-68`, `TextUnit.h:59-62` | `Orient::Upright`, `exactTangent`, `reserve=false` set directly, `kTateChuYoko` read back — each untested.
- nit | `kit/Gloss.h:34,37`, `kit/Grid.h:95`, `kit/Strokes.h:237` | `sunsetChromeType`/`silverChromeType` size-independence, `layouts::repeat`, `grooveRamp`'s `shoulder` — untested.
- nit | core features with no case: `balanceThroughLine`, `textDegradedBlocks/textComposing/reusedBlocks`, `cachePolicy`, `hexColor`, `toFill`, `frameOf`, `StampCache`, `transformOrigin3d`, `drawInkedImage` recorded-region and non-invertible paths (`BakeInk.h:172, 231`), `Region::oval`.
- nit | `core/Table.h`, `balanceChain`, `cellAlign`, `area` | tested only from `kit/test/ComposeTestLayoutsAndShapes.cpp:724-800`, the consuming tier | move under `core/test`.

## Counts

| severity | count |
|---|---|
| blocker | 4 |
| should-fix | 84 |
| nit | 96 |
| **total** | **184** |

The four blockers are each a one-to-three-line fix. The two Skia double-releases only fire on a failed wrap but corrupt the heap when they do; the Grid hang and the `PaintContext` truncation are reachable from ordinary authoring.
