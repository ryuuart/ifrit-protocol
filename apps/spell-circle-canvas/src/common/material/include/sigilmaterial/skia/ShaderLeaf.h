#pragma once

/** @file
 * @ingroup material-skia
 *
 * ShaderLeaf — a leaf that binds into a slot as a Skia shader. The
 * seam between the material tree and anything Skia can already shade: a
 * gradient a renderer built natively, a procedural shader Skia ships, a
 * composed material lowered to one shader. A subclass yields the shader
 * and compares itself by value. A `Texture` is not one: the executor
 * binds it as the image shader `shader(texture, frame)` builds.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Leaf.h>

namespace sigil::material::skia {

/** A leaf the Skia backend binds by asking for its shader. */
class ShaderLeaf : public Leaf {
 public:
  /** The shader bound into the slot; null binds nothing. */
  virtual sk_sp<SkShader> shader() const = 0;
  /** The shader bound into the slot of a material drawn at @p frame — a
   *  leaf whose pixels move with time answers the frame standing at
   *  `frame.seconds`; every other leaf answers `shader()`. */
  virtual sk_sp<SkShader> shaderAt(const FrameData& frame) const {
    (void)frame;
    return shader();
  }
};

}  // namespace sigil::material::skia
