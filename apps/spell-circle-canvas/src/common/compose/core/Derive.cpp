/** @file
 * Derive phase: content whose input is RESOLVED geometry — text exclusions
 * (flowAround) plumbed into SigilWeave, band spines and stroke gaps borrowed
 * from other keyed nodes, and connectors and rails routed between keyed
 * nodes' resolved bounds. Runs after Yoga has laid the tree out; a changed
 * exclusion asks the caller for another round of its bounded convergence
 * loop.
 *
 * This is also where reference cycles are rejected — nothing upstream checks
 * for them. Every borrow here resolves a key to an instance and then refuses
 * the answer if that instance is this node or one of its descendants: a
 * descendant's box is computed FROM this node's, so borrowing it would feed
 * the shape its own output and the loop would never settle. The refusal is
 * silent by design (draw nothing rather than diverge), which is the same
 * answer an unknown key gets.
 */

#include <include/core/SkContourMeasure.h>  // the connector's terminal gap
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>

#include <algorithm>
#include <boost/container/flat_set.hpp>
#include <cmath>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** Routed elements hit near their PATH, not their layout box (an inset(0)
 *  rail must not eclipse the scene): expand the route by a ±6px tolerance
 *  once at derive time; Query.cpp tests containment against it. */
SkPath expandForHit(const SkPath& route) {
  SkPaint p;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(12.0f);
  p.setStrokeCap(SkPaint::kRound_Cap);
  return skpathutils::FillPathWithPaint(route, p);
}

/** The shape a laid-out instance actually occupies, in its OWN space —
 *  the same answer the painter builds, so a borrowed spine and the
 *  element it was borrowed from can never disagree. */
SkPath resolvedShapeOf(Instance& inst) {
  const ElementNode& node = *inst.description;
  const SkRect rect = inst.owner->instanceRect(inst);
  const SkSize size{rect.width(), rect.height()};
  if (node.deriveData && !inst.connectorPath.isEmpty())
    return inst.connectorPath;
  if (node.shapeFn) return node.shapeFn(size);
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(size.width(), size.height()));
  return b.detach();
}

/** Whether that shape is a SILHOUETTE the node declared, as opposed to the
 *  rectangle `resolvedShapeOf` falls back to. Corner radii deliberately do
 *  not count: they round the fill, not the outline the borrow family reads,
 *  and `resolvedShapeOf` ignores them everywhere else too. */
bool hasResolvedSilhouette(const Instance& inst) {
  const ElementNode& node = *inst.description;
  return (bool)node.shapeFn ||
         (node.deriveData && !inst.connectorPath.isEmpty());
}

/** The cycle guard every borrow in this file applies: the target must not be
 *  this node or anything under it. Borrowing a DESCENDANT's geometry is the
 *  cycle — the child's box is derived from this node's, so the shape would
 *  feed itself and the layout loop would never settle. If you add another
 *  borrow, it needs this check too, or an author can hang the layout pass
 *  with a key. */
bool borrowIsCyclic(const Instance& inst, Instance* target) {
  for (Instance* p = target; p; p = p->parent)
    if (p == &inst) return true;
  return false;
}

}  // namespace

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

/** ONE STORY THROUGH AS MANY FRAMES AS IT WAS GIVEN.
 *
 *  Every other derived thing borrows a rect or a path from a node's BOX;
 *  a frame borrows where the frame before it STOPPED, which is an answer
 *  of that frame's own text layout rather than of its geometry. So the
 *  chain is walked here, in chain order, and each frame is laid out again
 *  at the measure it already resolved to before the next one is asked
 *  what it inherits — a chain of any length therefore settles inside this
 *  one pass, rather than one link per convergence round.
 *
 *  A CHAIN'S HEAD is a frame nothing threads into. A frame that threads
 *  into itself, or into a cycle, is dropped at the point the walk revisits
 *  it, which is the same rule the borrow family applies to a cyclic key. */
bool Composer::Impl::resolveThreads() {
  if (threadedInstances.empty()) return false;
  bool moved = false;
  // Which frames are somebody's target: the rest are chain heads.
  boost::container::flat_set<const Instance*> threadedInto;
  std::vector<Instance*> targets;
  for (Instance* inst : threadedInstances) {
    auto found = byKey.find(inst->description->textData->threadTo);
    if (found == byKey.end()) continue;
    if (threadedInto.insert(found->second).second)
      targets.push_back(found->second);
  }
  // A FRAME IS BOUNDED BY ITS OWN DEPTH, and the last link of a chain is a
  // frame although it threads nowhere: what it cannot hold has nowhere to
  // go, and unbounded it would draw past its box instead of running out
  // and taking the marker the leaf asked for. Only the walk knows which
  // leaf that is, so the fact is written onto the instance here — and off
  // again where a chain no longer reaches, which is what the kept list is
  // for.
  const auto bound = [&](Instance* frame, bool inChain) {
    if (frame->threadedInto == inChain) return;
    frame->threadedInto = inChain;
    frame->contentRev++;
    if (frame->yoga) YGNodeMarkDirty(frame->yoga);
    moved = true;
  };
  for (Instance* stale : threadTargets)
    if (!threadedInto.count(stale)) bound(stale, false);
  for (Instance* target : targets) bound(target, true);
  threadTargets = std::move(targets);
  boost::container::flat_set<const Instance*> visited;
  for (Instance* head : threadedInstances) {
    if (threadedInto.count(head)) continue;  // not a head
    uint32_t cursor = 0;
    // The story numbers its own lines, so each frame is told where in that
    // numbering its first line stands. Words, characters, sentences and
    // named runs are already the story's — every frame builds the whole
    // story's paragraph and resumes at a word — and the line is the one
    // address that was the frame's rather than the story's.
    uint32_t lineOffset = 0;
    std::vector<Instance*> chain;
    for (Instance* frame = head; frame;) {
      if (!visited.insert(frame).second)
        break;  // a cycle: stop where it closes
      const detail::TextData* text =
          frame->description && frame->description->textData
              ? &*frame->description->textData
              : nullptr;
      Instance* next = nullptr;
      if (text && !text->threadTo.empty()) {
        auto found = byKey.find(text->threadTo);
        if (found != byKey.end()) next = found->second;
      }
      // THE MEASURE THE NEXT FRAME SETS IN, for the widow rule: the lines
      // it counts are the remainder, and the remainder is what the next
      // frame will hold. Read off the box that frame resolved to on the
      // pass before this one — 0 the first time round, which is weave's
      // "not known" and leaves the count at this frame's own measure.
      // A frame that has not been laid out reports a measure that is not a
      // number; 0 is what weave reads as "not known", so the guard
      // compares what will be STORED and not the raw answer — comparing
      // the raw one is never equal, and the chain would report movement
      // every round until the convergence budget ran out.
      const float rawMeasure = next ? instanceRect(*next).width() : 0.0f;
      const float nextMeasure =
          std::isfinite(rawMeasure) && rawMeasure > 0 ? rawMeasure : 0.0f;
      if (frame->threadCursor != cursor ||
          frame->threadLineOffset != lineOffset ||
          frame->threadNextMeasure != nextMeasure) {
        frame->threadCursor = cursor;
        frame->threadLineOffset = lineOffset;
        frame->threadNextMeasure = nextMeasure;
        frame->contentRev++;
        if (frame->yoga) YGNodeMarkDirty(frame->yoga);
        moved = true;
      }
      // Re-fill at the box this frame RESOLVED to, so the next link reads
      // a remainder that belongs to this cursor. The box and not the
      // measure: a frame is bounded by its own depth, and the depth is an
      // answer of the layout that just ran.
      const SkRect box = instanceRect(*frame);
      if (box.isFinite() && box.width() > 0)
        layoutText(*frame, box.width(), box.height());
      cursor =
          frame->textLayout.overflowed()
              ? frame->textLayout.firstUnplacedWord
              : (frame->paragraph ? (uint32_t)frame->paragraph->words().size()
                                  : cursor);
      lineOffset += (uint32_t)std::max(frame->textLayout.lineCount, 0);
      chain.push_back(frame);
      frame = next;
    }
    // The story's own line count, which only the finished walk knows and
    // every frame of the chain needs: a cascade numbered over the story
    // spans the story's units, not the ones this frame happened to hold.
    for (Instance* link : chain) link->threadStoryLines = lineOffset;
    moved |= balanceRuns(chain);
  }
  return moved;
}

/** THE CHAIN FILLED FROM @p first AT ONE DEPTH: every frame of the run
 *  re-laid out at its own measure and this depth, each resuming where the
 *  one before it stopped.
 *
 *  It answers with how many lines the run placed and whether the last of
 *  them still had something left over — the two facts a depth is judged
 *  by — and it leaves the run laid out at that depth, so the caller ends
 *  by filling at the depth it chose. */
Composer::Impl::ChainFill Composer::Impl::fillRun(
    const std::vector<Instance*>& run, size_t first, size_t last, float depth,
    uint32_t cursor) {
  ChainFill filled;
  // A frame with no box to fill holds nothing, so a run that ENDS on one
  // has not been shown to hold what it was asked to hold: the verdict is
  // the last filled frame's, and a skipped tail overrides it.
  bool skippedTail = false;
  for (size_t i = first; i < last; ++i) {
    Instance* frame = run[i];
    const SkRect box = instanceRect(*frame);
    if (!box.isFinite() || box.width() <= 0) {
      skippedTail = true;
      continue;
    }
    skippedTail = false;
    frame->threadCursor = cursor;
    layoutText(*frame, box.width(), depth);
    filled.lines += (uint32_t)std::max(frame->textLayout.lineCount, 0);
    filled.overflowed = frame->textLayout.overflowed();
    cursor =
        filled.overflowed
            ? frame->textLayout.firstUnplacedWord
            : (frame->paragraph ? (uint32_t)frame->paragraph->words().size()
                                : cursor);
  }
  if (skippedTail) filled.overflowed = true;
  filled.cursor = cursor;
  return filled;
}

/** EVERY BALANCED RUN OF ONE CHAIN, SHORTENED TO WHAT IT HOLDS.
 *
 *  A run opens at a frame that declares it and closes before the next one
 *  that does. Its depth is found by halving: the frames' declared depth is
 *  the ceiling, nothing is the floor, and each trial fills the run again
 *  and asks whether it still holds what it was asked to hold — all of the
 *  story, or the story down to a stated line. The shallowest depth that
 *  does is the answer, and every frame of the run resolves to it.
 *
 *  A FIXED NUMBER OF HALVINGS rather than the exact turnover, so the
 *  answer is a hair deeper than the tightest one and the cost is bounded
 *  whatever the story is. The ceiling is the DECLARED depth and never the
 *  resolved one: read the resolved depth and each round would halve what
 *  the round before it chose, and the columns would close on nothing. */
bool Composer::Impl::balanceRuns(const std::vector<Instance*>& chain) {
  bool moved = false;
  for (size_t first = 0; first < chain.size(); ++first) {
    const detail::TextData* opens =
        chain[first]->description && chain[first]->description->textData
            ? &*chain[first]->description->textData
            : nullptr;
    if (!opens || !opens->balanceChain) continue;
    size_t last = first + 1;
    while (last < chain.size()) {
      const detail::TextData* text =
          chain[last]->description && chain[last]->description->textData
              ? &*chain[last]->description->textData
              : nullptr;
      if (text && text->balanceChain) break;
      ++last;
    }
    const Dim declared = chain[first]->description->layout.height;
    if (declared.unit != Dim::Unit::Px || declared.value <= 0) continue;
    const uint32_t cursor = chain[first]->threadCursor;
    const uint32_t through = opens->balanceThroughLine;
    // The line the run is asked to reach is the STORY's, so what the run
    // itself placed is counted from where the run starts in that
    // numbering — which is the one thing that lets a second run be asked
    // for a line number and not for a count of its own.
    const uint32_t opensAt = chain[first]->threadLineOffset;
    const auto holds = [&](float depth) {
      const ChainFill filled = fillRun(chain, first, last, depth, cursor);
      return through == ~0u ? !filled.overflowed
                            : opensAt + filled.lines > through;
    };
    float tooShallow = 0.0f;
    float deepEnough = declared.value;
    if (holds(deepEnough))
      for (int step = 0; step < kBalanceSteps; ++step) {
        const float trial = (tooShallow + deepEnough) * 0.5f;
        if (holds(trial))
          deepEnough = trial;
        else
          tooShallow = trial;
      }
    const ChainFill settled = fillRun(chain, first, last, deepEnough, cursor);
    // WHERE THE RUN STOPPED IS WHERE THE NEXT ONE STARTS: balancing moved
    // the boundary, and a frame below it that kept the pre-balance cursor
    // would resume at the wrong word.
    if (last < chain.size() && chain[last]->threadCursor != settled.cursor) {
      chain[last]->threadCursor = settled.cursor;
      chain[last]->contentRev++;
      if (chain[last]->yoga) YGNodeMarkDirty(chain[last]->yoga);
      moved = true;
    }
    for (size_t i = first; i < last; ++i) {
      if (!chain[i]->yoga) continue;
      if (std::abs(instanceRect(*chain[i]).height() - deepEnough) > 0.25f)
        moved = true;
      YGNodeStyleSetHeight(chain[i]->yoga, deepEnough);
    }
    first = last - 1;
  }
  return moved;
}

SkPath Composer::Impl::boundaryOutlineOf(Instance& target, float width,
                                         float height) {
  const ElementNode& node = *target.description;
  if (node.boundary == Boundary::Glyphs && node.kind == Kind::Text &&
      target.paragraph) {
    if (target.glyphOutlineRev != target.measuredRev) {
      target.glyphOutline = target.textLayout.glyphOutline();
      target.glyphOutlineRev = target.measuredRev;
    }
    if (!target.glyphOutline.isEmpty()) return target.glyphOutline;
  } else if (node.boundary == Boundary::Coverage && width > 0 && height > 0) {
    // Traced at the SAME device scale the painter traces at, so the two
    // readings are one cached answer and neither invalidates the other.
    const SkPath& traced = coverageOutline(target, {width, height}, hostScale);
    if (!traced.isEmpty()) return traced;
  }
  return hasResolvedSilhouette(target) ? resolvedShapeOf(target) : SkPath();
}

bool Composer::Impl::deriveFlow(Instance& inst) {
  bool relayout = false;
  const DeriveData* derive = &*inst.description->deriveData;

  if (inst.paragraph) {
    std::vector<Exclusion> exclusions;
    const SkRect own = absoluteRect(inst);
    for (const std::string& key : derive->flowAroundKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end()) continue;
      if (borrowIsCyclic(inst, it->second)) continue;
      Instance& target = *it->second;
      const SkRect box = absoluteRect(target);
      Exclusion exclusion;
      exclusion.bounds = box.makeOffset(-own.left(), -own.top());
      // WHAT THE TARGET SAYS ITS EDGE IS — the one property it already
      // carries for its own decorations, read here for the same answer:
      // its glyph outlines on a text leaf under `Boundary::Glyphs`, the
      // silhouette of what it DREW under `Boundary::Coverage` (a cut-out,
      // a clip, a mask, a photograph's alpha, at the tolerance
      // `Element::threshold` set), its declared shape otherwise, and its
      // box when it declares none. The margin means the same thing in
      // every case — a disc of that radius round whatever edge is being
      // subtracted — so a truer edge is never a second rule.
      SkPath boundaryPath =
          boundaryOutlineOf(target, box.width(), box.height());
      if (!boundaryPath.isEmpty()) {
        exclusion.path = boundaryPath.makeTransform(SkMatrix::Translate(
            box.left() - own.left(), box.top() - own.top()));
        SkRect oval = SkRect::MakeEmpty();
        // A round silhouette is subtracted ANALYTICALLY: the same answer,
        // at one square root per line band instead of a walk of a
        // flattened outline. Only a true circle qualifies — an ellipse's
        // inscribed circle is a different shape, and this is the one place
        // where taking the near-enough answer would be silently wrong.
        if (exclusion.path.isOval(&oval) &&
            std::abs(oval.width() - oval.height()) <= 0.01f) {
          exclusion.circle = true;
          exclusion.bounds = oval;
          exclusion.path.reset();
        }
      }
      exclusions.push_back(std::move(exclusion));
    }
    if (exclusions != inst.exclusionsLocal) {
      inst.exclusionsLocal = std::move(exclusions);
      inst.contentRev++;
      if (inst.yoga)  // positioned text re-measures via positionedRect
        YGNodeMarkDirty(inst.yoga);
      inst.markPaintDirtyUp();
      relayout = true;
    }
  }

  return relayout;
}

void Composer::Impl::deriveRoute(Instance& inst) {
  const DeriveData* derive = &*inst.description->deriveData;

  // band(around(key)): the spine is another element's resolved shape,
  // moved into this node's local space.
  if (!derive->bandAround.empty()) {
    SkPath spine;
    auto it = byKey.find(derive->bandAround);
    if (it != byKey.end() && !borrowIsCyclic(inst, it->second)) {
      const SkRect own = absoluteRect(inst);
      const SkRect target = absoluteRect(*it->second);
      spine = resolvedShapeOf(*it->second)
                  .makeTransform(SkMatrix::Translate(target.left() - own.left(),
                                                     target.top() - own.top()));
    }
    if (spine != inst.bandSpine) {
      inst.bandSpine = std::move(spine);
      inst.markPaintDirtyUp();
    }
  }

  // strand::from(key): the keyed PATHS a decoration borrows, in this
  // node's local space. Same walk, same cycle guard as every other borrow.
  if (!derive->borrowedPathKeys.empty()) {
    std::vector<std::pair<std::string, SkPath>> paths;
    paths.reserve(derive->borrowedPathKeys.size());
    const SkRect own = absoluteRect(inst);
    for (const std::string& key : derive->borrowedPathKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end() || borrowIsCyclic(inst, it->second)) continue;
      const SkRect target = absoluteRect(*it->second);
      paths.emplace_back(
          key, resolvedShapeOf(*it->second)
                   .makeTransform(SkMatrix::Translate(
                       target.left() - own.left(), target.top() - own.top())));
    }
    if (paths != inst.borrowedPaths) {
      inst.borrowedPaths = std::move(paths);
      inst.markPaintDirtyUp();
    }
  }

  // spans::fit(key): the boxes a stroke pass sizes its gap from.
  if (!derive->spanFitKeys.empty()) {
    std::vector<std::pair<std::string, SkRect>> rects;
    rects.reserve(derive->spanFitKeys.size());
    const SkRect own = absoluteRect(inst);
    for (const std::string& key : derive->spanFitKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end() || borrowIsCyclic(inst, it->second)) continue;
      SkRect target = absoluteRect(*it->second);
      target.offset(-own.left(), -own.top());
      rects.emplace_back(key, target);
    }
    if (rects != inst.spanFitRects) {
      inst.spanFitRects = std::move(rects);
      inst.markPaintDirtyUp();
    }
  }

  if (!derive->connectFrom.empty() && !derive->connectTo.empty()) {
    auto fromIt = byKey.find(derive->connectFrom);
    auto toIt = byKey.find(derive->connectTo);
    if (fromIt != byKey.end() && toIt != byKey.end()) {
      SkRect own = absoluteRect(inst);
      SkRect from = absoluteRect(*fromIt->second);
      SkRect to = absoluteRect(*toIt->second);
      from.offset(-own.left(), -own.top());
      to.offset(-own.left(), -own.top());
      if (from != inst.connectorFrom || to != inst.connectorTo) {
        inst.connectorFrom = from;
        inst.connectorTo = to;
        if (derive->router) {
          inst.connectorPath = derive->router(from, to);
        } else {
          SkPathBuilder b;
          b.moveTo(from.centerX(), from.centerY());
          b.lineTo(to.centerX(), to.centerY());
          inst.connectorPath = b.detach();
        }
        // Terminal gap, the connector's spelling of the same knob rail
        // anchors carry: pull each END of the routed path back along
        // itself, clamped like the rail's pullIn below so a short wire
        // keeps a visible run. Applied to the ROUTE rather than to the
        // rects, so it works for any router — straight, orthogonal, arc.
        if (derive->connectorGap > 0 && !inst.connectorPath.isEmpty()) {
          SkPathBuilder trimmed;
          SkContourMeasureIter iter(inst.connectorPath, false);
          bool touched = false;
          while (sk_sp<SkContourMeasure> contour = iter.next()) {
            const float len = contour->length();
            if (len <= 0) continue;
            if (contour->isClosed()) {  // no terminals to pull back
              (void)contour->getSegment(0, len, &trimmed, true);
              continue;
            }
            const float pull = std::min(derive->connectorGap, len * 0.45f);
            (void)contour->getSegment(pull, len - pull, &trimmed, true);
            touched = true;
          }
          if (touched) inst.connectorPath = trimmed.detach();
        }
        inst.routedHitPath = expandForHit(inst.connectorPath);
        inst.markPaintDirtyUp();
      }
    }
  }

  if (derive->railAnchors.size() >= 2) {
    std::vector<SkPoint> pts;
    pts.reserve(derive->railAnchors.size());
    const SkRect own = absoluteRect(inst);
    bool resolvedAll = true;
    for (const Anchor& anchor : derive->railAnchors) {
      if (anchor.nodeKey.empty()) {  // a free waypoint, bound to nothing
        pts.push_back(anchor.point);
        continue;
      }
      auto it = byKey.find(anchor.nodeKey);
      if (it == byKey.end()) {
        resolvedAll = false;
        break;
      }
      bool cyclic = false;  // the rail must not thread itself
      for (Instance* p = it->second; p; p = p->parent)
        if (p == &inst) {
          cyclic = true;
          break;
        }
      if (cyclic) {
        resolvedAll = false;
        break;
      }
      const SkRect target = absoluteRect(*it->second);
      pts.push_back(
          {target.left() + target.width() * anchor.norm.x() - own.left(),
           target.top() + target.height() * anchor.norm.y() - own.top()});
    }
    if (!resolvedAll) {
      // An anchor vanished (station unmounted) or went cyclic: the rail
      // goes with it — a stale path pointing at ghosts must not replay.
      if (!inst.connectorPath.isEmpty()) {
        inst.connectorPath.reset();
        inst.routedHitPath.reset();
        inst.railPoints.clear();
        inst.markPaintDirtyUp();
      }
    } else if (pts.size() >= 2) {
      // Terminal gaps: pull the rail's ends back along their segments,
      // clamped so short segments keep a visible run (and a two-point rail
      // pulled from both ends can't invert).
      auto pullIn = [](SkPoint& end, const SkPoint& next, float gap) {
        if (gap <= 0) return;
        SkVector d = next - end;
        const float len = d.length();
        if (len < 1e-3f) return;
        const float pull = std::min(gap, len * 0.45f);
        d.scale(pull / len);
        end += d;
      };
      pullIn(pts.front(), pts[1], derive->railAnchors.front().gap);
      pullIn(pts.back(), pts[pts.size() - 2], derive->railAnchors.back().gap);

      if (pts != inst.railPoints) {
        inst.railPoints = std::move(pts);
        if (derive->railRouter) {
          inst.connectorPath = derive->railRouter(inst.railPoints);
        } else {
          SkPathBuilder b;  // default: the straight polyline
          b.moveTo(inst.railPoints.front());
          for (size_t i = 1; i < inst.railPoints.size(); ++i)
            b.lineTo(inst.railPoints[i]);
          inst.connectorPath = b.detach();
        }
        inst.routedHitPath = expandForHit(inst.connectorPath);
        inst.markPaintDirtyUp();
      }
    }
  }
}

}  // namespace sigil::compose
