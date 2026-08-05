// The KIT tier's translation unit.
//
// The kit is header-only VALUES over compose's public seams, and this file
// exists to make that a structural fact rather than a promise: it is
// compiled as its own library (SigilComposeKit) whose ONLY include path is
// compose's `include/` tree. ComposeInternal.h and ComposeRuntime.h live in
// the source root, which is on no target's include path, so a kit header
// that reached for the kernel's internals would fail to compile here.
//
// See kit/BoundaryProbe.cpp for the negative control, and CMakeLists.txt
// for how to run it.

#include <sigilcompose/kit/Divisions.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kit.h>
#include <sigilcompose/kit/Legibility.h>
#include <sigilcompose/kit/PixelType.h>
#include <sigilcompose/kit/Strokes.h>

namespace sigil::compose::kit {

/** Anchors the library so it is not an empty archive, and asserts the two
 *  properties the tier boundary is FOR: a kit value is comparable (it can
 *  be handed to the reconciler) and it satisfies the same seam concept a
 *  user-written value would. If a kit value ever stops being a peer of a
 *  hand-written one, this stops compiling. */
bool kitLinked() {
  static_assert(ShaperScheme<brush::shapers::Wave>);
  static_assert(ShaperScheme<brush::shapers::Jitter>);
  static_assert(ShaperScheme<brush::shapers::Offset>);
  static_assert(ProfileScheme<brush::shapers::Wave>);
  return true;
}

}  // namespace sigil::compose::kit
