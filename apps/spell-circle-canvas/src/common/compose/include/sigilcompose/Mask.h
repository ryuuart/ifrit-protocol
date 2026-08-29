#pragma once

/** @file
 * SigilCompose masking family — Region, the `parts::` selection of a node's
 * paint outputs, the `by::` gates that say how that paint arrives, and
 * Mask, the pairing of the two that `Element::mask` stacks.
 */

#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "sigilcompose/Motion.h"
#include "sigilcompose/Shape.h"
#include "sigilcompose/Stroke.h"

namespace sigil::compose {

namespace detail {
struct Instance;
}  // namespace detail

class Material;

// ---------------------------------------------------------------------------
// THE MASKING FAMILY — `mask(by::…)` and `mask(parts::…, by::…)`
//
// Appearance-gating is a relation between a SELECTION and a GATE, and the
// two questions are independent:
//
//   parts::  says WHICH of this node's paint outputs the mask applies to
//   by::     says HOW that paint arrives — the rule by which it is cut
//
// Keeping them independent is what makes the family small: any selection
// combines with any gate, so a region cut, an arc-length reveal, a
// coverage matte and a per-mark cut are one mechanism rather than four.
//
// TWO LAWS, and they are the whole semantics:
//
//  - **A gate is a SHOW set.** `by::edge(90, 0.3)` shows 30%;
//    `spans::upTo(t)` shows [0,t]. The complement is a separate TERM,
//    never a mode flag — `by::outside(r)` is the word for the outside of
//    a region — so which way round a mask runs is readable at the call
//    site without chasing an argument.
//  - **Stacked masks INTERSECT where their selections overlap.** Both must
//    pass. Nesting already means this everywhere else (clip inside clip,
//    a span claim under a whole-node cut), and UNION is spelled inside one
//    gate value (combining spans with `|`), never across gates. Each mask keeps
//    its OWN animation slots, so three masks on one node may run at three
//    different rates and the intersection is exact per frame.

/** A COMPARABLE region in the node's own local space — the shape gate's
 *  value, and deliberately a closed vocabulary rather than an
 *  `std::function<SkPath(SkSize)>`.
 *
 *  A callable would defeat the point of the gate. A gate is read live,
 *  every frame; an incomparable generator never participates in reconciler
 *  equality, so a masked node could never prune and would re-record
 *  forever. A Region is a value: it compares, it prunes, and a node masked
 *  by one still qualifies for the memo that lets an animated-scalar node
 *  hold its recording between ticks.
 *
 *  `own()` is the node's own shape — the region `clip()` uses, and the
 *  reason clip() survives as sugar over this. */
class Region {
 public:
  enum class Kind : uint8_t {
    Own,   ///< the node's own shape() / corners box
    Rect,  ///< a rectangle in local coordinates
    Oval,  ///< the oval inscribed in a local rectangle
    Path,  ///< an explicit local path (SkPath is a comparable value)
  };

  /** The node's own silhouette — clip()'s region, as a value. */
  static Region own();
  static Region rect(const SkRect& r);
  static Region oval(const SkRect& bounds);
  /** An explicit path in the node's LOCAL space. Comparable (SkPath has
   *  structural equality), so this is the general escape hatch that still
   *  prunes — unlike a generator. */
  static Region path(SkPath p);

  Kind kind() const { return m_kind; }
  bool operator==(const Region& other) const;

  /** The path this region covers, given the node's own silhouette. */
  SkPath resolve(const SkPath& ownShape) const;

 private:
  Kind m_kind = Kind::Own;
  SkRect m_rect = SkRect::MakeEmpty();
  SkPath m_path;

  /** FIELD PIN (see ComposeInternal.h's FIELD PINS block). A Region rides
   *  inside a mask gate, which is read LIVE every frame — a region that
   *  compares equal when it isn't leaves a pruned node revealing to its
   *  first frame forever. */
  static void fieldPin(Region& v) {
    auto& [kind, rect, path] = v;
    static_assert(
        std::tuple_size_v<decltype(std::tie(kind, rect, path))> == 3,
        "Region gained or lost a member — rule on it in Region::operator== "
        "(Compose.cpp, in the arm of the Kind that reads it), then bump this "
        "count.");
  }
};

/** WHICH of a node's paint outputs a mask applies to — a small comparable
 *  value, combined with `|`.
 *
 *  The outputs, in paint order (`Paint.cpp`'s own list):
 *
 *      backgrounds · background passes │ fill · echo │ overlays │
 *      content │ children │ foregrounds · foreground passes
 *
 *  which collapse into four classes an author can name: the SURFACE (the
 *  fill and its echoes), the MARKS (every decoration in every slot and
 *  every span pass, across BOTH z-halves — a boundary does not have two of
 *  itself), the CONTENT leaf, and the CHILDREN.
 *
 *  `named()` addresses ONE mark by the local label its slot call gave it.
 *  Those are the same LOCAL names `stroke(Spans, what, name)` carries:
 *  one element's labels for its own marks, for inspection and
 *  intra-element reference. They are NOT query keys — `Composer::bounds`
 *  and `hitTest` do not see them — because a second identity system beside
 *  `key()` is exactly what the query side refuses.
 *
 *  A name that matches nothing selects nothing, silently, exactly as
 *  `spans::rest("unknown")` and `spans::fit("unknown")` do. */
class Parts {
 public:
  enum Bits : uint8_t {
    kSurface = 1u << 0,
    kMarks = 1u << 1,
    kContent = 1u << 2,
    kChildren = 1u << 3,
    kAll = kSurface | kMarks | kContent | kChildren,
  };
  uint8_t bits = 0;
  /** parts::named(): local mark labels, in declaration order. */
  std::vector<std::string> names;

  bool operator==(const Parts&) const = default;
  bool selects(Bits what) const { return (bits & what) != 0; }
  /** Does this selection reach a mark carrying `label` (possibly empty)? */
  bool selectsMark(std::string_view label) const {
    if (bits & kMarks) return true;
    if (label.empty()) return false;
    for (const std::string& n : names)
      if (n == label) return true;
    return false;
  }
  /** Everything this node paints — the one-argument mask()'s selection. */
  bool isEverything() const { return (bits & kAll) == kAll; }
};

/** Union: `parts::content() | parts::surface()`. */
inline Parts operator|(Parts a, const Parts& b) {
  a.bits = (uint8_t)(a.bits | b.bits);
  a.names.insert(a.names.end(), b.names.begin(), b.names.end());
  return a;
}

/** The selection factories — the WHICH half of `mask(what, with)`. */
namespace parts {
/** Every output, children included. The one-argument `mask(by::…)` means
 *  this, and it is what the docs lead with. */
Parts all();
/** Every decoration in every slot and every span pass, BOTH z-halves —
 *  backgrounds, overlays, foregrounds, background passes, stroke passes. */
Parts marks();
/** The fill surface (and its echo re-stamps). */
Parts surface();
/** The text / image / custom leaf. */
Parts content();
Parts children();
/** ONE mark, by the local name its slot call gave it. */
Parts named(std::string_view name);
}  // namespace parts

class Gate;

/** The gate factories — the HOW half of `mask(what, with)`. Named `by::`
 *  because the call site reads as English: mask by edge, mask by spans,
 *  mask by shape. */
namespace by {
/** ARC LENGTH along the node's boundary — the same `Spans` value the
 *  stroke slot uses, here answering "how much of this exists yet" rather
 *  than "where does this pass go".
 *
 *      .mask(by::spans(spans::upTo(animate(from(0.f).to(1.f), {600ms}))))
 *
 *  A boundary is a 1-D coordinate, so this gate addresses only the paint
 *  that TRACES the boundary — the surface and the marks. Selecting content
 *  or children with it means nothing and DOES NOTHING, silently. */
Gate spans(Spans where);
/** A MOVING STRAIGHT EDGE at `angleDeg` across the node's laid-out box,
 *  showing the fraction lying before it (0 = left-to-right, 90 = top to
 *  bottom, 180 = right-to-left, 270 = from the bottom). This is the gate
 *  that reveals a filled surface by EXTENDING it — an arc-length window
 *  walks the perimeter instead, and scaleX/scaleY squash rather than
 *  reveal. */
Gate edge(float angleDeg, Animatable<float> fraction);
/** A REGION of the node's local space, kept. `by::shape(Region::own())` is
 *  what `clip()` does. */
Gate shape(Region r);
/** …and its complement: everything OUTSIDE the region. Two masks
 *  intersect, so a set difference is `by::shape(a)` and `by::outside(b)`
 *  on one node. */
Gate outside(Region r);
/** A COVERAGE SOURCE: the selected paint keeps the Material's ALPHA — the
 *  soft-edged mask (a gradient fade, a noise dissolve, a stencil sprite).
 *
 *  Costs a `saveLayer` per masked group, so it is the expensive member of
 *  the family; `spans`, `edge` and `shape` ride path effects and clips. */
Gate alpha(Material coverage);
/** …and its complement, a term of its own exactly as `outside` is: the
 *  selected paint keeps what the Material does NOT cover. After Effects'
 *  Alpha Inverted Matte. Costs nothing beyond `alpha` — the coverage layer
 *  composites with `kDstOut` instead of `kDstIn`, which is `1 - a` exactly
 *  and needs no shader. */
Gate alphaOut(Material coverage);
/** The other coverage source: the selected paint keeps the Material's
 *  LUMA. After Effects' Luma Matte — paint a matte in greys (or in
 *  anything) and its brightness is the coverage.
 *
 *  **The luma law**: `Y' = 0.299 R' + 0.587 G' + 0.114 B'` — Rec. 601
 *  coefficients on the ENCODED values, taken on the PREMULTIPLIED colour.
 *  Compose paints into surfaces with no colour space attached, so a
 *  shader's channels are the display-encoded numbers the author wrote and
 *  there is no linear stage to weight. Rec. 601's luma coefficients are
 *  the set defined on gamma-encoded R'G'B'; Rec. 709's 0.2126/0.7152/
 *  0.0722 are LUMINANCE coefficients defined on linear light and do not
 *  belong here. Premultiplied means a TRANSPARENT matte reads as black
 *  and hides, as AE's does — a half-transparent white and an opaque 50%
 *  grey are the same matte.
 *
 *  Same cost as `alpha` plus one SkSL pass over the coverage layer, and
 *  none at all when the Material resolves to a colour, where the
 *  weighting is one dot product in C++. */
Gate luma(Material coverage);
/** …and ITS complement: the selected paint keeps what the Material's luma
 *  leaves DARK. After Effects' Luma Inverted Matte. */
Gate lumaOut(Material coverage);
}  // namespace by

/** HOW paint arrives past a mask — a comparable value built by the `by::`
 *  factories. Only the members its Kind reads are meaningful; the rest
 *  keep their defaults so the value compares. */
class Gate {
 public:
  enum class Kind : uint8_t { Spans, Edge, Shape, Coverage };
  /** Coverage: WHICH channel of the Material becomes coverage. The two
   *  members are one mechanism — the same `saveLayer` and the same
   *  compositing pass — so they are a field of one Kind and not two Kinds.
   *  See `by::alpha` / `by::luma` for the law each names. */
  enum class Channel : uint8_t { Alpha, Luma };
  Kind kind = Kind::Spans;
  Spans where;                        ///< Spans
  float angleDeg = 0.0f;              ///< Edge
  Animatable<float> fraction = 1.0f;  ///< Edge
  Region region;                      ///< Shape
  /** Shape AND Coverage: keep the COMPLEMENT of what this gate names —
   *  `by::outside`, `by::alphaOut`, `by::lumaOut`. One field because it is
   *  one question ("which side of the show set?"), asked of two kinds. */
  bool outside = false;
  Channel channel = Channel::Alpha;  ///< Coverage
  /** Coverage. Held out of line because Material is declared in its own
   *  header, which includes this one. */
  std::shared_ptr<const Material> coverage;

  /** Structural equality. Declared here and defined beside the
   *  reconciler's own property comparator, so an animated fraction
   *  compares the way every other animated property does. */
  bool operator==(const Gate& other) const;
  /** How many animatable floats this gate contributes, in the order
   *  `Instance::maskAnims` indexes them: three per Spans term (begin, end,
   *  offset), one for an Edge fraction, none for Shape or Coverage (a
   *  Region is static and a Material animates itself). */
  size_t valueCount() const;
};

/** One mask: a selection and a gate. */
struct Mask {
  Parts what;
  Gate with;
  bool operator==(const Mask& other) const {
    return what == other.what && with == other.with;
  }
};

}  // namespace sigil::compose
