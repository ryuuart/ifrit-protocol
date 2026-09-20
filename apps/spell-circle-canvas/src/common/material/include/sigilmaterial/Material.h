#pragma once

/** @file
 * @ingroup material-core
 *
 * The umbrella over the core feature: recipes, materials, programs and
 * the values between them. Backends are included by their own headers.
 */

/** @defgroup material-core The recipe seam
 *  A recipe is a material's definition — the parameter struct that is its
 *  ABI, one shader body per language, the slots it samples and the frame
 *  values it reads. A material is one instance of it, and resolving an
 *  instance against a frame answers a compiled program plus the bytes to
 *  upload (core/Recipe.h, core/Material.h, core/Program.h). */
/** @defgroup material-color Colour
 *  The leaf the rest is built over: the four-float colour a parameter
 *  struct holds, the OKLab, OKLCH and CIELAB round trips, the ramp as one
 *  value, the harmonies read around a hue, the palette a picture is made
 *  of, and the threshold a pixel is dithered against (color/Color.h). */
/** @defgroup material-pattern Procedural patterns
 *  A tile baked once from a program and repeated under a mapping, the
 *  stock tile generators over it, and the cloth a sett and a weave make
 *  (pattern/Tile.h, pattern/Patterns.h, pattern/Weave.h). */
/** @defgroup material-texture Textures
 *  An image and how it is sampled, as a comparable value filling a
 *  recipe's slot: the texture itself, the folders an authoring tool
 *  exports by role, the environment a reflective surface sees, the bevel
 *  normals an outline implies, and the atlas a sheet is cut into
 *  (texture/Texture.h, texture/TextureSet.h). */
/** @defgroup material-field Fields
 *  Surfaces evaluated per pixel rather than baked as a tile: the halftone
 *  ramp, Perlin noise, luminance grain, the tube overlay and the ripple
 *  (field/Field.h). */
/** @defgroup material-sdf Signed distance
 *  Shape, border, glow and soft shadow in one pass over a signed
 *  distance, one recipe per silhouette kind and every style parameter a
 *  uniform (sdf/Sdf.h). */
/** @defgroup material-mask Masks
 *  Where on a surface something applies, as a material whose output is
 *  read as a scalar — what `over()` consults to decide how much of a
 *  stacked material shows at a point (mask/Mask.h). */
/** @defgroup material-skia The Skia paint
 *  The Skia-facing half of the library: a small tree of paint nodes that
 *  compiles to one shader, the post-processing effect over a rendered
 *  layer, the colour bridge, and the one-call path fill
 *  (skia/Paint.h, skia/Effect.h, skia/Color.h, skia/Draw.h). */
/** @defgroup material-slang The Slang backend
 *  Slang source compiled to SPIR-V, the reflected layout saying where
 *  each uniform's bytes go, and the buffer a draw's uniforms are written
 *  into at those offsets (slang/SlangCompiler.h). */
/** @defgroup material-ocio Colour management
 *  OpenColorIO view transforms baked to materials, answering an empty LUT
 *  material in a build that found no OpenColorIO (ocio/Ocio.h). */
/** @defgroup material-stock The stock shaders
 *  Every recipe this library ships, enumerated, and the one call that
 *  warms them before the first frame asks (stock/Stock.h). */
/** @defgroup material-kit The kit
 *  Presets over the primitives: named ramps and skies, the
 *  metallic-roughness surface, grained and reflective surfaces, the
 *  pattern panels, the layer-style tables and the animated text paints
 *  (kit/Recipes.h). */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Bank.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Leaf.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/Target.h>
#include <sigilmaterial/core/Terms.h>
#include <sigilmaterial/core/UniformBlock.h>
