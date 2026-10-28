#pragma once

/** @file
 * Internal to the kernel — the recorded-command-list tier behind the
 * kernel's bake seam: what a picture bake is taken over, and the
 * operations that take, replay and drop one.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRect.h>
#include <sigiltree/cache/Bake.h>

#include "Instance.h"

class SkCanvas;

namespace sigil::compose {

/** WHAT THE PICTURE TIER BAKES: the node, the canvas its recording is
 *  replayed onto, and the paint context the recording freezes in — the
 *  leaf blend and opacity a recording bakes rather than applies, and the
 *  content scalars it was recorded from. */
struct PictureBakeTarget {
  Composer::Impl* painter = nullptr;
  detail::Instance* inst = nullptr;
  SkCanvas* canvas = nullptr;
  float hostScale = 1;
  SkBlendMode leafBlend = SkBlendMode::kSrcOver;
  float leafOpacity = 1;
  detail::Instance::ContentScalars* scalars = nullptr;
  /** The matrix the recording's ops reach the DEVICE through when it is
   *  replayed this frame — the canvas's own matrix composed out through
   *  every enclosing recording. A recording holding a device-space bake is
   *  exact under this one matrix and is remade when it differs. */
  SkMatrix deviceMatrix = SkMatrix::I();
  /** Whether that matrix is the one the node was drawn under last frame.
   *  The outermost recording hands it to every device bake inside it as
   *  the "holding still" verdict those bakes cannot observe for themselves,
   *  being painted only when the recording is. */
  bool matrixStable = true;
  /** The device clip the recording's ops were cut to. An ink clip inside
   *  one names whole device pixels, so a recording holding a device bake
   *  is exact for the clip it was made under and is remade under
   *  another. */
  SkIRect deviceClip = SkIRect::MakeEmpty();
};

/** THE RECORDED-COMMAND-LIST TIER, behind the kernel's bake seam. Taking
 *  it, replaying it and dropping it are this library's — an SkPicture is a
 *  Skia value and its cull rect, its recording depth and the leaf paint it
 *  freezes are all compose's rules. Whether to take one THIS FRAME is not:
 *  that is the kernel's three-way answer over what the proof said and what
 *  the node is holding, and the pixel tiers beside this one ask it in
 *  exactly the same words.
 *
 *  Stateless, so every instance of it is the same value. */
struct PictureBake : tree::BakeOperations<PictureBakeTarget> {
  void take(PictureBakeTarget& t) const override;
  void replay(PictureBakeTarget& t) const override;
  void drop(PictureBakeTarget& t) const override;
  [[nodiscard]] bool held(const PictureBakeTarget& t) const override;
  // Stateless: every instance of it is the same value. (A defaulted
  // comparison would compare the abstract base subobject, which has none.)
  bool operator==(const PictureBake&) const { return true; }
};

}  // namespace sigil::compose
