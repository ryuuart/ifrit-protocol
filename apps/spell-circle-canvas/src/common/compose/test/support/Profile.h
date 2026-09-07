#pragma once
// The profile fixtures: a wrapper that keeps its subject painted every
// frame so it is profiled at all, the row lookup that proves it was, and a
// whole-surface pixel grab for frame-to-frame comparisons. The expensive
// panel those are usually pointed at is stroked, so it lives with the brush
// support (BrushTestSupport.h).

#include <sigilmaterial/skia/Paint.h>

#include <string>
#include <vector>

#include "Effects.h"
#include "Host.h"

namespace {

/** THE CACHE-TEST FIXTURE. Use it for anything that asserts on how a node
 *  was cached.
 *
 *  A static node under a CACHEABLE parent is painted exactly once, into
 *  that parent's recording, and never visited again — so it never appears
 *  in `profile()`, and any assertion written as "loop the rows, check the
 *  matches" passes vacuously over an empty match set. `Cache::None` on the
 *  wrapper keeps the subject painted every frame, which is also how these
 *  nodes sit in real scenes: under a stack() with animated siblings.
 *
 *  Pair it with `requireRow()`, and note what each half guarantees:
 *  `profiledUnder` gets the node PROFILED, `requireRow` proves it was.
 *  **Neither makes it PROMOTED** — that needs the node to be genuinely
 *  over the cost threshold, which is a property of the content, not of
 *  the fixture. See `expensivePanel`. */
Element profiledUnder(Element subject) {
  return box().cache(Cache::None).child(std::move(subject));
}

/** The profile row for `key`, failing loudly when there is none — the
 *  difference between "the library did not promote it" and "the test never
 *  looked at it", which a `for`-loop-with-an-`if` cannot tell you. */
const Composer::NodeCost* requireRow(const Composer& composer,
                                     const char* key) {
  for (const auto& row : composer.profile())
    if (row.label.rfind(key, 0) == 0) return &row;
  ADD_FAILURE() << "no profile row for '" << key
                << "' — the node was never painted, so nothing below this "
                   "line tested anything. Wrap it in profiledUnder().";
  return nullptr;
}

/** The profile row for the node keyed `key`, or nullptr — the labels a
 *  profile carries are "<key> (<kind> WxH)". Unlike `requireRow`, an
 *  absent row is an answer here: a case that asserts a node was NOT
 *  profiled asks for exactly this. */
const Composer::NodeCost* rowOf(Host& host, const char* key) {
  const std::string prefix = std::string(key) + " (";
  for (const Composer::NodeCost& row : host.composer.profile())
    if (row.label.rfind(prefix, 0) == 0) return &row;
  return nullptr;
}

std::vector<SkColor> grab(Host& host) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  std::vector<SkColor> out;
  out.reserve(size_t{200} * 200);
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) out.push_back(bm.getColor(x, y));
  return out;
}

}  // namespace
