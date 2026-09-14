/** @file
 * The functions that start an Element — the containers, the three text
 * content forms and the frame over a story, the image, the custom
 * program in both spellings, the routed connector and rail, the slot —
 * and the makers behind layout() and memo().
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilimage/asset/ImageAsset.h>

#include <any>
#include <functional>
#include <string>

#include "ComposeInternal.h"

namespace sigil::compose {

using detail::Kind;

Element box() { return {}; }

Element stack() {
  Element e;
  e.node()->kind = Kind::Stack;
  return e;
}

Element positioned() {
  Element e;
  e.node()->layout.positioned = true;
  return e;
}

Element text(Utf8 utf8) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData& text = e.node()->textData.ensure();
  text.utf8 = utf8.bytes();
  // Set in the font and ink in force where the leaf lands in the tree:
  // the cascade pass resolves them and materialises the paragraph from
  // the instance's font, and `style` here is never read.
  text.inherits = true;
  return e;
}

Element text(Utf8 utf8, sigil::weave::TextStyle style) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData& text = e.node()->textData.ensure();
  text.utf8 = utf8.bytes();
  text.style = std::move(style);
  // The box fits the type: measured text must not stretch on the cross
  // axis. That demotion is NOT applied here — it happens when layout properties
  // are written to Yoga, where the alignment this leaf actually resolved to
  // (its own, or its parent's) is known, so a parent's alignItems(Center)
  // or alignItems(End) still reaches text leaves untouched.
  return e;
}

Element text(sigil::weave::RichText spans) {
  // A run written with a NAME and no sheet on the value resolves through
  // the sheet in force where the leaf lands, when the leaf is shaped; a
  // value that names its own sheet keeps it.
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData& text = e.node()->textData.ensure();
  // A rich text started with no base is set in the font in force where it
  // lands, exactly as a plain leaf that names no style is; one started
  // with a base is set in that base alone.
  text.inherits = !spans.hasBase();
  // The base rides along as `style` because everything downstream that asks
  // a text leaf what it is set in — the strut a line height comes from, the
  // metric band textFill() maps into — reads one style, and a mixed
  // paragraph's answer to that question is the style its unstyled runs use.
  text.style = spans.base();
  text.rich = std::move(spans);
  return e;
}

Element frame(sigil::weave::Story story) {
  Element e = text(story.content());
  const std::span<const sigil::weave::ParagraphStyle> blocks = story.blocks();
  if (!blocks.empty())
    e.paragraphs(std::vector<sigil::weave::ParagraphStyle>(blocks.begin(),
                                                           blocks.end()));
  return e;
}

Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData& text = e.node()->textData.ensure();
  text.paragraphOverride = std::move(paragraph);
  text.layoutOptions = std::move(options);
  return e;
}

Element image(std::shared_ptr<const sigil::image::ImageAsset> asset) {
  Element e;
  e.node()->kind = Kind::Image;
  e.node()->imageData.ensure().asset = std::move(asset);
  return e;
}

Element image(sk_sp<SkImage> picture, Fit fit) {
  if (!picture) return box();
  const float w = (float)picture->width();
  const float h = (float)picture->height();
  Element leaf = image(std::make_shared<const sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(std::move(picture))));
  // THE FIT IS LAYOUT AND NOT A MATRIX: the leaf takes the box it stands
  // in, and where its proportions are kept the node itself is the right
  // shape — so what is painted is the whole of the node and a caller
  // computes nothing.
  if (fit == Fit::Stretch || w <= 0.0f || h <= 0.0f)
    return leaf.width(pct(100)).height(pct(100));
  leaf.aspect(w / h);
  if (fit == Fit::Contain)
    leaf.width(pct(100)).maxWidth(pct(100)).maxHeight(pct(100));
  else
    leaf.height(pct(100)).minWidth(pct(100));
  return leaf;
}

Element custom(PaintProgram program) {
  Element e;
  e.node()->kind = Kind::Custom;
  e.node()->customData.ensure().program = std::move(program);
  return e;
}

Element custom(std::string_view key, PaintProgram program) {
  Element e = custom(std::move(program));
  e.node()->customData->key = std::string(key);
  return e;
}

Element picture(sk_sp<SkPicture> recorded, SkSize native) {
  if (!recorded) return box();
  const SkSize recordedAt = native;
  // The picture's own id and the size it was recorded at are the
  // identity: a recording cannot change, so two describes handing over
  // the same one at the same native size are the same drawing — and a
  // keyed custom is compared by its key alone, so a native size left out
  // of it would prune two different scalings together.
  Element e = custom("picture:" + std::to_string(recorded->uniqueID()) + ":" +
                         std::to_string(native.width()) + "x" +
                         std::to_string(native.height()),
                     [pic = std::move(recorded), recordedAt](
                         SkCanvas& canvas, const PaintContext& ctx) {
                       SkAutoCanvasRestore restore(&canvas, true);
                       if (recordedAt.width() > 0 && recordedAt.height() > 0)
                         canvas.scale(ctx.size.width() / recordedAt.width(),
                                      ctx.size.height() / recordedAt.height());
                       canvas.drawPicture(pic.get());
                     });
  return e.width(Dimension(native.width())).height(Dimension(native.height()));
}

Element pathFigure(SkPath absolute, float bleed) {
  SkRect bounds = absolute.getBounds();
  bounds.outset(bleed, bleed);
  SkPath local = absolute.makeTransform(
      SkMatrix::Translate(-bounds.left(), -bounds.top()));
  return box().absolute().rect(bounds).shape(heldPath(std::move(local)));
}

Element connector(std::string_view fromKey, std::string_view toKey,
                  Router router, float gap) {
  Element e;
  e.node()->kind = Kind::Custom;  // painted via derive-resolved outline
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.connectFrom = std::string(fromKey);
  derive.connectTo = std::string(toKey);
  derive.router = std::move(router);
  derive.connectorGap = gap;
  // A wire is routed between two settled BOXES, one at each end.
  derive.reads.push_back({derive.connectFrom, sigil::core::Facet::Bounds});
  derive.reads.push_back({derive.connectTo, sigil::core::Facet::Bounds});
  return e;
}

Element rail(std::vector<Anchor> anchors, RailRouter router) {
  Element e;
  e.node()->kind = Kind::Custom;  // painted via the derive-routed outline
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.railAnchors = std::move(anchors);
  derive.railRouter = std::move(router);
  // …and a rail through as many boxes as it has waypoints. A free point
  // is bound to nothing and reads nothing.
  for (const Anchor& anchor : derive.railAnchors)
    if (!anchor.key().empty())
      derive.reads.push_back(
          {std::string(anchor.key()), sigil::core::Facet::Bounds});
  return e;
}

Element slot(std::string_view name) {
  Element e;
  e.node()->kind = Kind::Slot;
  e.node()->key = std::string(name);
  return e;
}

namespace detail {
Element makeLayout(std::function<std::vector<SkRect>(const LayoutInput&)> place,
                   bool readsChildMinSizes) {
  Element e;
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.placeFn = std::move(place);
  derive.placeReadsMinSizes = readsChildMinSizes;
  return e;
}

Element makeMemo(std::any properties,
                 std::function<bool(const std::any&, const std::any&)> equal,
                 std::function<Element(const std::any&)> invoke) {
  Element e;
  detail::MemoData& memo = e.node()->memoData.ensure();
  memo.properties = std::move(properties);
  memo.equal = std::move(equal);
  memo.invoke = std::move(invoke);
  // Captured HERE, in the author's scope — the whole point. By the time
  // the reconciler decides whether to call `invoke`, this stack is gone.
  memo.environment = core::environment::capture();
  return e;
}

}  // namespace detail

}  // namespace sigil::compose
