#pragma once

/** @file
 * SigilCompose factories — the functions that start an Element: `box`,
 * `stack`, `positioned`, `text` in its three content forms and `frame`
 * over a story, `image`, `picture`, `pathFigure`, `custom`, `layout`,
 * `slot` and `memo`.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/style/Style.h>

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::weave {
// The two composed text values the text factories take, defined in
// <sigilweave/paragraph/RichText.h> and <sigilweave/layout/Story.h>.
class RichText;
class Story;
}  // namespace sigil::weave

namespace sigil::compose {

/** UTF-8 std::string → std::u8string for text() call sites. */
inline std::u8string toU8(std::string_view s) {
  return std::u8string(s.begin(), s.end());
}

// ---- factories -----------------------------------------------------------

Element box();
/** Overlap container: children share the box, painted in (zIndex,
 *  declaration order). EVERY child is absolute — the container sets it
 *  after the child's own layout props, so a child cannot rejoin the flex
 *  flow from inside a stack (it keeps its insets, which is what absolute
 *  is for: `.top(12).right(12)` pins a corner). Mixed flow wants a box
 *  with a stack inside it. */
Element stack();
/** A container whose children carry their OWN rects and skip Yoga
 *  entirely — no flex nodes anywhere below it. Generated geometry
 *  (tilings, lattices, node graphs, fields drawn as real elements) never
 *  wants layout, and this is how to say so.
 *
 *  The child spelling is the ordinary placement longhand:
 *  `.left(x).top(y).width(w).height(h)` — px, or pct() against the
 *  parent's rect; an open width/height with an opposing `.right()`/
 *  `.bottom()` pins the far edge instead; a text leaf with an open
 *  extent measures against its resolved (or the parent's) width.
 *  Rects nest: a child's children position inside ITS rect, the whole
 *  subtree Yoga-free. Everything else about the children is ordinary —
 *  decorations, strokes, masks, transitions, stagger, zIndex, hitTest,
 *  bounds() — because instances still exist; only their layout engine is
 *  gone. A large generated field therefore costs one Yoga node rather
 *  than one per element.
 *
 *  The container ITSELF is an ordinary box in its parent's flow: size it
 *  with dims or insets, because it does NOT auto-size from its children.
 *  NOT SUPPORTED INSIDE, and ignored silently when written: flex props,
 *  centerAt, layout() schemes, flowAround text. Those need the flex
 *  world. */
Element positioned();
Element text(std::u8string utf8, sigil::weave::TextStyle style);
/** Mixed-style text as a COMPARABLE VALUE — see weave::RichText. A re-described
 *  identical value prunes, which is the whole difference between this and
 *  the pointer overload below. */
Element text(sigil::weave::RichText spans);
/** ONE FRAME OF A STORY — a text leaf over `story`'s content and block
 *  styles, which `key()` names and `thread()` links to the next.
 *
 *  Every frame of a chain declares the same story, and the chain decides
 *  which part of it each one holds. See `weave::Story` and `Element::thread`.
 */
Element frame(sigil::weave::Story story);

/** Full-control text: a prebuilt Paragraph (spans, mixed styles) plus
 *  ParagraphLayoutOptions (justification, hyphenation, Knuth–Plass,
 *  overflow…). The paragraph is shared by reference: reuse one
 *  shared_ptr across renders to keep shaping caches warm; a fresh
 *  pointer means "content changed" and re-shapes. */
Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options = {});
Element image(std::shared_ptr<const sigil::image::ImageAsset> asset);
/** A box whose content is one paint program (≡ box().background(p)).
 *
 *  TWO COSTS AN AUTHOR MUST KNOW. First, it is cached like any static
 *  subtree, so a program that reads the clock — or changes for any other
 *  reason without a re-describe — MUST declare `.cache(Cache::None)`, or
 *  its first frame is recorded and replayed frozen. Second, the program is
 *  an incomparable callable, so the structural prune cannot prove a
 *  custom() node unchanged and it re-records on every render(). Wrap it in
 *  memo(), keep its Element value stable across renders, or use the keyed
 *  overload below. Value decorations (PathFormat, Slice, Shadow) prune
 *  automatically — prefer them for static chrome.
 *
 *  IT SIZES LIKE AN EMPTY BOX, and the failure is silent. Being literally
 *  `box().background(p)`, a custom() leaf has no intrinsic size: dropped
 *  into an `absolute().inset(0)` parent it stretches on the cross axis and
 *  measures ZERO on the main one, so the program runs against a
 *  zero-height context and draws nothing at all. Give it dims, or make it
 *  `absolute().inset(0)` itself — which is exactly what
 *  `instancing::instances()` returns, for exactly this reason. */
Element custom(PaintProgram program);
/** The PRUNABLE spelling: the key is the program's IDENTITY, on the same
 *  author contract as `shapes::parametric(key, …)`. One key must always
 *  name one drawing at one parameterisation — fold anything that varies
 *  into the key, or two different pictures compare equal and the stale one
 *  replays. Two describes with equal keys compare EQUAL and the node
 *  prunes; the unkeyed form above re-records every render(). */
Element custom(std::string_view key, PaintProgram program);

/** A RECORDED PICTURE AS A LEAF — the door out of a bake.
 *
 *  `snapshot()` hands back an `SkPicture` and `image()` takes an
 *  `ImageAsset`, so a caller who has baked a subtree has, until now, had
 *  to draw it back through `custom()`. That forfeits exactly what the
 *  bake was taken for: an unkeyed program is incomparable, so its node
 *  re-records every describe, and a caller who reaches for
 *  `Cache::None` to be safe gives up the caching too.
 *
 *  This is that leaf, and it prunes: a picture's identity is its own,
 *  and two describes handing over the same picture compare equal. A
 *  recorded picture cannot change, so the node is static by
 *  construction and caches like any other static subtree.
 *
 *  @p native is the size the picture was recorded at, and it becomes the
 *  node's own size, so a snapshot drops into a layout without being
 *  measured again. Give the node other dims and the picture is SCALED to
 *  them — stretched, both axes independently, since a picture has no
 *  aspect to preserve on the caller's behalf. A null picture is an empty
 *  box. */
Element picture(sk_sp<SkPicture> recorded, SkSize native);

/** A LEAF THE SHAPE OF A PATH ALREADY IN CANVAS COORDINATES.
 *
 *  A figure worked out in the drawing's own frame — a projected ray, a
 *  traced region, a swept arc — is an absolute path, and a node is a box
 *  with a local shape. Placing one by hand is the same four lines every
 *  time: take the bounds, grow them by whatever the mark will spend
 *  outside the line, set the node's rect to that, and re-base the path so
 *  its origin is the box's corner.
 *
 *  This is those four lines. @p bleed grows the box on all sides, in px,
 *  and must cover half the stroke width the caller is about to dress it
 *  with plus anything the mark spends beyond that — an under-grown box
 *  clips the mark. The node is `absolute`, so its rect is read against
 *  whatever it hangs under rather than joining a flex flow, and it
 *  carries a `heldPath` shape, so
 *  it prunes as long as the path handed in is one the caller holds rather
 *  than one rebuilt each describe.
 *
 *  It has NO FILL and no mark of its own: dress it with `.fill()`, a
 *  `.stroke()` or a `foreground(PathFormat{…})` as the drawing wants. */
Element pathFigure(SkPath absolute, float bleed = 0.0f);

/** A container whose children are placed by @p scheme instead of
 *  flexbox (nests freely inside flex and vice versa). The container
 *  itself is sized by its own dims/flex; children are measured by
 *  Yoga/SigilWeave, then positioned and sized by scheme.place() in a
 *  bounded second layout pass. */
template <LayoutScheme L>
Element layout(L scheme);

namespace detail {
Element makeLayout(
    std::function<std::vector<SkRect>(const LayoutInput&)> place);
}  // namespace detail

template <LayoutScheme L>
Element layout(L scheme) {
  return detail::makeLayout(
      [s = std::move(scheme)](const LayoutInput& in) { return s.place(in); });
}

/** A named mount point whose content is supplied independently via
 *  `Composer::renderSlot()`. The surrounding tree is not re-described when
 *  the slot updates, so its caches stay valid — this is how two data
 *  domains that change at different rates share one tree.
 *
 *  THE NAME IS STORED AS THE ELEMENT'S `key`, the same field `.key()`
 *  writes: `slot("hud").key("panel")` is a slot named "panel", and
 *  `renderSlot("hud")` then finds nothing and does nothing. Name the slot
 *  here and only here; `.key()` warns once if called on one anyway. */
Element slot(std::string_view name);

namespace detail {
Element makeMemo(std::any props,
                 std::function<bool(const std::any&, const std::any&)> equal,
                 std::function<Element(const std::any&)> invoke);
}  // namespace detail

/** Deferred description: `fn` runs only when `props` changed (by
 *  operator==) since the last render on this position/key — AND the
 *  ambient `env::` bindings are unchanged, because a memo is a pure
 *  function of (props, environment) and would otherwise serve the theme
 *  it first described under forever. The captured stack is re-established
 *  around the deferred call, so `env::inherited<T>()` inside `fn` reads
 *  what was bound where the memo was WRITTEN, not where it runs.
 *
 *  THE SHELL. The element this returns is a shell; the element `fn`
 *  produces is the node's whole look. Three calls on the shell speak for
 *  the node: `.key()` names it (the reconciler matches memos by the
 *  shell's key), and `.cache()` and `.bakeScale()` say how the produced
 *  subtree is held — both are carried onto the produce, and an explicit
 *  choice on the shell wins over one made inside `fn`. Every other
 *  property set on the shell (a fill, a transform, a layout dimension, a
 *  child, a decoration) describes nothing, is warned about once and
 *  ignored: set it on the element produced inside `fn`. A `.cache()`
 *  changed while the props and the environment compare equal does not
 *  take, because a hit reuses the retained produce untouched — change a
 *  prop to re-describe. */
template <ComponentProps P, ComponentFn<P> F>
Element memo(P props, F fn) {
  return detail::makeMemo(
      std::any(std::move(props)),
      [](const std::any& a, const std::any& b) {
        return std::any_cast<const P&>(a) == std::any_cast<const P&>(b);
      },
      [fn = std::move(fn)](const std::any& p) -> Element {
        return fn(std::any_cast<const P&>(p));
      });
}

}  // namespace sigil::compose
