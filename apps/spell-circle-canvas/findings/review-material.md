# Merge-readiness review — SigilMaterial (sub-pass)

Read-only pass over `src/common/material` (delegated by the
geometry+material+world reviewer; delivered directly). Paths relative to
`apps/spell-circle-canvas/`. Category in brackets: 1 correctness, 2 API,
3 README drift, 4 comment rules, 5 leftovers, 6 duplication, 7 file
size, 8 test gaps.

## Findings, most severe first

- blocker | `src/common/material/skia/Ramp.cpp:46` | `skia::unitRamp` builds `Paint::linear({0,0},{0,1},…)`, a gradient one node-local pixel tall, then clamps; the header (`skia/Ramp.h:33`) promises the stops over the unit square top to bottom, and `compose/kit/Eras.cpp:171,175` use it for chrome text fills that therefore paint the last stop flat | use `Paint::linearUnit`, the unit-square factory that exists for this [1]
- blocker | `material/core/Material.cpp:266` | `if (s.leaf && !(*s.leaf == *o.leaf))` dereferences `o.leaf` without the null-parity check the material branch at 264 has; `Material::child(name, shared_ptr<const Leaf>{})` stores a null | check `(s.leaf != nullptr) != (o.leaf != nullptr)` first [1]
- blocker | `material/mask/shaders/MaskConstant.sksl:2` | `return half4(shape(value))` puts the coverage in alpha; `Mask.cpp:6-8` states both bodies return it in all three colour channels at full alpha, and the Slang twin returns `float4(v,v,v,1)` — a mask drawn alone is transparent, not grey | `half v = shape(value); return half4(v,v,v,1.0);` [1]
- blocker | `material/mask/shaders/MaskSampled.sksl:15` | the same divergence from `MaskSampled.slang:15` | return the scalar in rgb at alpha 1 [1]
- should-fix | `material/skia/Effect.cpp:606` | `Effect::then` precomposes into one `SkImageFilter` whenever neither side `isAnimated()`, discarding `m_children`, so a geometry-dependent child (a `uResolution` sigma map, `blur()`'s box crop) is frozen at the null-context snapshot and `usesWorldSpace()` reports false | keep the chain nodes when either side has a context-needing child [1]
- should-fix | `material/kit/Surface.cpp:29` | `body.find("REFLECTION_WEIGHT")` used unguarded at 30's `replace`; npos throws | guard `at != npos`, as `field/Field.cpp:27` does [1]
- should-fix | `material/field/Field.cpp:119` | `grainRecipe()` lazily fills a plain static array with no lock and returns a reference into it, while the two peer caches (`skia/Paint.cpp:286`, `core/Combine.cpp:208`) take a mutex | lock the fill [1]
- should-fix | `material/core/Combine.cpp:209` | the composed-recipe cache keys on raw `const Recipe*` and holds no reference to the operands, so a freed operand whose address is reused hands back the previous composition — the hazard `skia/Paint.cpp:283-285` guards by holding the shared_ptr | key on `shared_ptr<const Recipe>` [1]
- should-fix | `material/skia/Paint.cpp:996` (and 1307/1332/1359/1386) | `Paint::uniform` appends without de-duplication while `Paint::child` at 1026-1035 replaces by name; setting a uniform twice grows the vector and breaks equality | replace an existing entry by name [2]
- should-fix | `material/skia/Effect.cpp:535` (543, 552, 562, 570; `m_bound` at 470/482/494) | `Effect::uniform` has the same append-without-replace while `Effect::child` de-duplicates | last write wins [2]
- should-fix | `include/sigilmaterial/skia/Paint.h:424` | the `UniformBlock` overload doc cites "(Compose.h)", a downstream library's header, and the wrong location | drop the citation [4]
- should-fix | `include/sigilmaterial/skia/Paint.h:636` | "(see the AnimatedDecoration concept)" cites a concept in SigilCompose, a consumer | delete the parenthetical [4]
- should-fix | `include/sigilmaterial/skia/Paint.h:766` | the `fieldPin` static_assert message, a shipping string, cites "(Paint.cpp: …)" | say the rule [4]
- should-fix | `include/sigilmaterial/skia/Effect.h:377` | "FIELD PIN (see ComposeInternal.h's FIELD PINS block). operator== is hand-written in Effects.cpp" cites a private compose header and names the wrong file | rewrite without file references [4]
- should-fix | `include/sigilmaterial/skia/Effect.h:350` | the `m_children` comment says the type is held by shared_ptr because `Material` is only forward-declared; the member type is `Paint` and both headers are included | correct the type and drop the false claim [4]
- should-fix | `material/skia/Paint.cpp:874` | "per the Patterns.h one-effect rule" cites another header for the rule | state it [4]
- should-fix | `material/core/shaders/Shading.slang:15` | "transcribed once, in Terms.h" is wrong: the C++ transcription lives in `sigilgeometry/mesh/render/Shading.h:148-190` | state that a host tier transcribes it, without a file [4]
- should-fix | `material/core/shaders/Shading.slang:124` | `fresnelRough`/`environmentBrdf`/`environmentSpecular`/`attenuate`/`luminance` duplicated line for line in C++ in SigilGeometry with no cross-check | a parity test over both spellings, or generate one from the other [6]
- should-fix | `material/kit/shaders/GrainedPrelude.slang:1` | `hashG/vnoiseG/fbm3G` are a byte re-spelling of `NoisePrelude.sksl:10-28` in a library that already strips one text into two languages (`core/Terms.cpp:23`) | write the noise once and strip it for SkSL [6]
- should-fix | `material/field/shaders/Grain.sksl:1` | a third `valueNoise` in the library with different arithmetic from the kit prelude's function of the same name | one value noise, shared [6]
- should-fix | `material/color/Ramp.cpp:23` | `bracket()` re-implements the stop search, end clamps and equal-position hard edge that `sampleRamp` performs at `color/Color.h:411-430`, comment included | one bracket helper both call [6]
- should-fix | `material/mask/Mask.cpp:72` | `maskVertexColor(colors, channel)` is `return maskMap(...)` — a second public name for one behaviour | delete it and its README/header entries [2]
- should-fix | `material/skia/Paint.cpp:892`, `1482` | `Paint::blend` and `foldBlend` read `m_amount` only for `i >= 1`, so `.amount()` on the first layer is silently discarded and the header documents only the mode exception | honour it or document it, in one shared fold body [2]
- should-fix | `material/skia/Paint.cpp:78` | `Live::lastInputs/lastShader` are `mutable` on a `shared_ptr<Live>` that Paint copies share, and `build()` writes them; the file states two composers may paint on two threads | guard the memo (also `Backed::lastKey/lastShader` at 84) [1]
- should-fix | `include/sigilmaterial/color/Ramp.h:69` | stops documented "in order" but nothing validates or sorts them; `bracket()` reads `front()/back()` as the extremes | report once on an out-of-order list, or sort on construction [1]
- should-fix | `material/texture/EnvironmentMap.cpp:416` | `prefilterSize()` clamps the default to a lower bound of 256, upsampling a 64- or 128-wide panorama; the user-set path at 414 has no such bound | clamp only above [1]
- should-fix | `material/texture/Atlas.cpp:215` | a tag whose range misses `[0, count)` registers an empty sequence and still sets `tagged = true`, skipping the "all" fallback | reject a tag whose range does not intersect [1]
- should-fix | `material/kit/shaders/Surface.sksl:1` | the SkSL twin of `kit.surface` ignores roughness, metallic, normal scale, transmission, ior, thickness and absorption, all of which the Slang body honours, and `kit/Surface.h` says nothing | state the split in the header [3]
- should-fix | `material/cmake/SlangEmbedSpirv.cmake:55` | the generated header is a declared OUTPUT written with `copy_if_different`, so an unchanged header keeps an old mtime and the rule re-runs every build | `file(TOUCH)` after the copy, or a stamp file [1]
- should-fix | `material/cmake/Slang.cmake:166` | `DEPENDS` never includes modules reachable through `INCLUDE`, so an imported module's change does not recompile the importer | append each include dir's `.slang` files to DEPENDS [1]
- should-fix | `material/skia/Paint.cpp:772` | `detail::unitRamp` silently truncates past 256 stops while the header advertises any number | report once, or state the ceiling [1]
- should-fix | `material/skia/Effect.cpp:220` | `warnUndeclaredEffectUniform` stops recording past 16 names and returns before warning, so the 17th is never reported | warn, then stop recording [5]
- should-fix | `include/sigilmaterial/skia/Paint.h:16,392,636` | three doc sites say "a ch::Output-bound uniform"; the API takes `motion::Animatable<float>` | spell the type the signature takes [3]
- should-fix | `include/sigilmaterial/core/UniformBlock.h:32` | a core header says "matching PixelBuffer", a class in the skia feature above it | drop the cross-reference [4]
- should-fix | `material/skia/Paint.cpp` (1573), `skia/Paint.h` (771), `skia/Effect.cpp` (748), `skia/test/SkiaTest.cpp` (947), `core/test/CoreTest.cpp` (708), `texture/test/TextureTest.cpp` (690), `kit/test/KitTest.cpp` (686) | one subject per file | split by subject: `Paint.cpp`/`PaintBuild.cpp`/`PaintSources.cpp`; `PixelBuffer.h`; `EffectBlurs.cpp`; one test file per suite [7]
- should-fix | tests missing: `Paint::sweep` (the clamp-not-wrap warning), `Paint::conical`, `Paint::buffer` and `PixelBuffer` (prune until committed), `skia::unitRamp`/`verticalRamp` (which is why the blocker went unnoticed), MedianCut on empty/single-colour/over-count inputs, a one-stop and an unsorted `Ramp`, harmonies across the 360/0 wrap (`rotateHue` never normalises) [8]
- nit | `material/skia/Paint.cpp:877` | `auto [effect, err] = MakeForShader(...)` discards `err`, so a Mix.sksl failure is silent | log it [5]
- nit | `material/kit/Recipes.cpp:31`, `kit/Surface.cpp:67-69`, `field/Field.cpp:162` | `SkSurfaces::Raster(...)->getCanvas()` with no null check | check the surface [1]
- nit | `material/texture/Surface.cpp:63` | an explicit RGBA_8888 bitmap written through the N32 shift macros; agrees only where `SK_R32_SHIFT` is 0 | index the bytes directly [1]
- nit | `material/skia/SkiaCompiler.cpp:56` | `uncommented()` leaves the last character of an unterminated block comment unblanked | blank to the end [1]
- nit | `material/ocio/Ocio.cpp:172` | `n` computed before a branch that may not use it | move it [5]
- nit | `include/sigilmaterial/skia/Color.h:37` | `skia::toColors` referenced only by the README and a test | delete unless a consumer is planned [5]
- nit | `include/sigilmaterial/skia/Paint.h:107` | "the MaterialX `<ramp>` atom" cited as justification | drop [4]
- nit | `include/sigilmaterial/field/Field.h:5` | the file summary omits `crtOverlay` | list all five [3]
- nit | `material/README.md` (1227 lines) | twelve feature targets in one document | a colour chapter and a paint chapter beside it [7]
- nit | `include/sigilmaterial/pattern/Tile.h:16`, `kit/TextPaint.h`, `texture/*.h` | Skia headers included outside the `sigilmaterial/skia/*` seam the README's boundary carves out | state the exception or forward-declare [3]

## Counts

- blocker: 4
- should-fix: 47
- nit: 11
