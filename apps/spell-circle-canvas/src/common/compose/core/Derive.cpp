/** @file
 * Derive phase: content whose input is RESOLVED geometry. Runs after Yoga
 * has laid the tree out — the pass over the flat instance lists, and the
 * tethers it resolves first so a connector that ends on a tethered box
 * routes to where it came to rest. The families it drives are beside it:
 * FlowAround.cpp, Threads.cpp and Routes.cpp.
 */

#include "ComposeRuntime.h"
#include "DeriveInternal.h"

namespace sigil::compose {

using namespace detail;

/** The derive pass over the flat instance lists the key index rebuilds each
 *  render: flowAround text nodes first, then routed nodes, both in tree
 *  order — no tree recursion here. Returns true when a text exclusion
 *  changed, which means the geometry the caller just laid out is stale and
 *  layout must run again. */
bool Composer::Impl::resolveDerived() {
  bool relayout = false;
  for (Instance* inst : flowInstances) relayout |= deriveFlow(*inst);
  relayout |= resolveThreads();
  // Tethers before routes: a tethered box IS geometry, and a connector or
  // a rail that ends on one must route to where it came to rest and not to
  // where layout left it.
  relayout |= resolveTethers();
  for (Instance* inst : routedInstances) deriveRoute(*inst);
  return relayout;
}

/** EVERY TETHERED BOX HUNG WHERE IT FITS.
 *
 *  The stated tether is tried, then each fallback in the order it was
 *  written, and the first placement that lies inside its container is
 *  taken; when none does, the stated one stands, so a box that fits
 *  nowhere is still placed where it was asked for rather than dropped.
 *
 *  The arithmetic is `Tether::place` and is not repeated here — the text
 *  kit hangs an object off a word through the same call, so an object
 *  tied to a box and an object tied to a word cannot come to rest by two
 *  different rules.
 *
 *  Everything is computed in COMPOSER space, where every anchor's rect can
 *  be read whatever subtree it sits in, and written back in the box's own
 *  parent's space, which is where Yoga's absolute insets are measured. */
bool Composer::Impl::resolveTethers() {
  bool moved = false;
  for (Instance* inst : tetheredInstances) {
    if (!inst->yoga) continue;  // inside a positioned() subtree: no Yoga node
    const Tether& stated = *inst->description->deriveData->tether;
    const SkRect own = instanceRect(*inst);
    const SkSize box{own.width(), own.height()};
    const SkRect canvas = SkRect::MakeSize(size);
    SkRect stood = SkRect::MakeEmpty();
    bool anyAnchor = false;
    bool fitted = false;
    const auto hang = [&](const Tether& tether) {
      auto it = byKey.find(tether.key);
      if (it == byKey.end() || borrowIsCyclic(*inst, it->second)) return;
      const SkRect placed = tether.place(absoluteRect(*it->second), box);
      if (!anyAnchor) {  // the stated placement, kept in case none fits
        anyAnchor = true;
        stood = placed;
      }
      const SkRect within = tether.within.isEmpty() ? canvas : tether.within;
      if (!within.contains(placed)) return;
      stood = placed;
      fitted = true;
    };
    hang(stated);
    for (const Tether& fallback : stated.fallbacks) {
      if (fitted) break;
      hang(fallback);
    }
    if (!anyAnchor) continue;  // an unknown key is silent

    const SkRect frame =
        inst->parent ? absoluteRect(*inst->parent) : SkRect::MakeSize(size);
    const float left = stood.left() - frame.left();
    const float top = stood.top() - frame.top();
    // Reported as a change only on an actual delta: the converging loop
    // keys off this, and an idempotent write must read as a quiet round or
    // every frame burns the full round count.
    if (std::abs(own.left() - left) > 0.25f ||
        std::abs(own.top() - top) > 0.25f)
      moved = true;
    YGNodeStyleSetPositionType(inst->yoga, YGPositionTypeAbsolute);
    YGNodeStyleSetPosition(inst->yoga, YGEdgeLeft, left);
    YGNodeStyleSetPosition(inst->yoga, YGEdgeTop, top);
    YGNodeStyleSetPosition(inst->yoga, YGEdgeRight, YGUndefined);
    YGNodeStyleSetPosition(inst->yoga, YGEdgeBottom, YGUndefined);
  }
  return moved;
}

}  // namespace sigil::compose
