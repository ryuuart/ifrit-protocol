/** @file
 * The KIT tier's translation unit.
 *
 * The kit is header-only VALUES over compose's public seams, and this file
 * exists to make that a structural fact rather than a promise: it is
 * compiled as its own library (SigilComposeKit) whose ONLY include path is
 * compose's `include/` tree. ComposeInternal.h and ComposeRuntime.h live in
 * core/, which is on no other target's include path, so a kit header
 * that reached for the kernel's internals would fail to compile here.
 *
 * See kit/BoundaryProbe.cpp for the negative control, and CMakeLists.txt
 * for how to run it.
 */

#include <sigilgeometry/kit/Divisions.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kit.h>
#include <sigilcompose/kit/Legibility.h>
#include <sigilcompose/kit/PixelType.h>

#include <concepts>

namespace sigil::compose::kit {

/** Anchors the library so it is not an empty archive, and asserts the
 *  property the tier boundary is FOR: a kit value is comparable, so it can
 *  be handed to the reconciler and pruned like a hand-written one. If a kit
 *  value ever stops being a peer of a hand-written one, this stops
 *  compiling. */
bool kitLinked() {
  static_assert(std::equality_comparable<geometry::path::Frame>);
  static_assert(std::equality_comparable<geometry::path::Grid>);
  static_assert(std::equality_comparable<geometry::shapes::Ticks>);
  static_assert(std::equality_comparable<geometry::shapes::TicksShape>);
  return true;
}

}  // namespace sigil::compose::kit
