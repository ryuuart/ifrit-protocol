#pragma once

/** @file
 * @ingroup material-skia
 *
 * THE PAINT'S SKIA EXECUTOR: what a `material::Paint` becomes when a Skia
 * canvas draws it — ONE `sk_sp<SkShader>`, layers folded through
 * `SkShaders::Blend` rather than a stacked saveLayer — and the sources
 * only Skia can hand a paint: a decoded image, a caller-owned raster, an
 * SkSL runtime effect, a raw shader. Beside them sit the two crossings a
 * paint's own words make into Skia's: a blend mode and a repeat.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmaterial/skia/Pass.h>
#include <sigilmaterial/skia/PixelBuffer.h>  // the source buffer() takes

#include <glm/mat3x3.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class SkImage;

namespace sigil::material::skia {

/** @name Sources only Skia can supply
 *  Each is a `material::Paint` like any other once built.
 *  @{ */
/** AN IMAGE OR A SPRITE as a paint, repeated past its edges as
 *  @p horizontal and @p vertical say; @p local maps source px into the
 *  node's space (a sprite's atlas sub-rect is a translate and a scale). */
Paint image(sk_sp<SkImage> image, Repeat horizontal = Repeat::Pad,
            Repeat vertical = Repeat::Pad, const SkMatrix& local = SkMatrix::I(),
            SkSamplingOptions sampling = {});
/** CONTENT THAT CHANGES WITHOUT RE-DESCRIBING: a caller-owned raster the
 *  paint samples — a simulation, a decoded video frame, a paint surface,
 *  a scrollback. Own the PixelBuffer, draw into it, `commit()`. The
 *  recipe compares by (source, revision), so an identical re-describe
 *  between commits PRUNES and the first describe after a commit patches
 *  exactly once. */
Paint buffer(std::shared_ptr<PixelBuffer> source, Repeat horizontal = Repeat::Pad,
             Repeat vertical = Repeat::Pad, const SkMatrix& local = SkMatrix::I(),
             SkSamplingOptions sampling = {});
/** An SkSL runtime effect as a paint. @p constants set named float
 *  parameters once; `Paint::bind` binds live ones and `Paint::slot`
 *  fills declared `uniform shader` sockets. The body's own reads set the
 *  tier: `uTime` or `uContentScale` is LIVE, `uResolution` alone is the
 *  cheaper GEOMETRY tier.
 *  @trap A null effect — a failed `MakeForShader` passed straight in —
 *  is a paint of nothing, said once. */
Paint sksl(sk_sp<SkRuntimeEffect> effect,
           std::vector<std::pair<std::string, float>> constants = {});
/** A raw shader as a paint (interop, escape). It compares by pointer. */
Paint paint(sk_sp<SkShader> shader);
/** A MATERIAL AS ONE PAINT: its base, with each layer blended over the
 *  accumulation, mixed back by its opacity and applied through its mask.
 *  The surface and the effects are not part of a paint. */
Paint paint(const Material& material);
/** A PAINT AS A MATERIAL'S BASE — the bridge for a source only this
 *  executor supplies (an image, a caller-owned buffer, a runtime effect)
 *  into the model, where it stacks under and over other materials. */
Material base(Paint paint);
/** @} */

/** @name What a paint becomes
 *  @{ */
/** THE SNAPSHOT: always a shader — a solid becomes a colour shader —
 *  which is what a blend composes. For a live paint it builds a fresh
 *  shader sampling bound values at their CURRENT readings: a snapshot,
 *  not a binding. Null for a paint of nothing. */
sk_sp<SkShader> shader(const Paint& paint);
/** THE PER-DRAW SHADER: for a live paint, rebuilt from the bound values
 *  and @p frame; for a geometry-dependent one, built against the frame's
 *  box; for a static one, exactly `staticShader(paint)`.
 *  @trap Null for a solid and for nothing — ask `isSolid()` and
 *  `isNone()` first. */
sk_sp<SkShader> shader(const Paint& paint, const FrameData& frame);
/** The STATIC snapshot: the shader a non-live paint already holds, or
 *  null for a solid, for nothing, and for a paint that needs a frame. */
sk_sp<SkShader> staticShader(const Paint& paint);
/** THE PASS RESOLVE — what a text runtime calls for a pass track's paint,
 *  once per draw: the recipe specialized to `in.units`, the instance's
 *  values, bindings and slots, and the runtime's own slots filled from
 *  @p in. Null when the paint is not recipe-backed or its specialization
 *  does not compile, so a broken pass shows resting letters rather than
 *  nothing. */
sk_sp<SkShader> resolvePass(const Paint& paint, const PassInputs& in,
                            const FrameData& frame);
/** @} */

/** @name The crossings into Skia's words
 *  @{ */
/** @p mode as Skia's. */
SkBlendMode toSkBlendMode(BlendMode mode);
/** Skia's @p mode as this library's. */
BlendMode toBlendMode(SkBlendMode mode);
/** @p repeat as Skia's tile mode: `Pad` clamps, `None` decals. */
SkTileMode toSkTileMode(Repeat repeat);
/** A frame's column-major 3×3 as Skia's matrix. */
SkMatrix toSkMatrix(const glm::mat3& matrix);
/** Skia's matrix as a frame's column-major 3×3 — what `FrameData::world`
 *  takes from a canvas's node-to-root transform. */
glm::mat3 toMatrix(const SkMatrix& matrix);
/** @} */

}  // namespace sigil::material::skia
