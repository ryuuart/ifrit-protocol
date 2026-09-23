/** @file
 * The functions that start an Element — the containers, the three text
 * content forms and the frame over a story, the image, the custom
 * program in both spellings, the figure a path becomes, the slot and the
 * point — and the makers behind layout() and memo().
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

namespace {
/** A fresh text leaf, which every `text()` form and `frame()` shape. */
Text textLeaf() {
  Text leaf{std::make_shared<detail::ElementNode>()};
  leaf.node()->kind = Kind::Text;
  return leaf;
}
}  // namespace

Element box() { return {}; }

Element stack() {
  Element e;
  e.node()->kind = Kind::Stack;
  return e;
}

Element positioned() {
  Element e;
  // What kind of container the node is, not a property it states.
  e.node()->fields.defaults().layout.positioned = true;
  return e;
}

Text text(Utf8 utf8) {
  Text e = textLeaf();
  detail::TextData& text = e.node()->textData.ensure();
  text.utf8 = utf8.bytes();
  // Set in the font and ink in force where the leaf lands in the tree:
  // the cascade pass resolves them and materialises the paragraph from
  // the instance's font, and `style` here is never read.
  text.inherits = true;
  return e;
}

Text text(Utf8 utf8, sigil::weave::TextStyle style) {
  Text e = textLeaf();
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

Text text(sigil::weave::RichText spans) {
  // A run written with a NAME and no sheet on the value resolves through
  // the sheet in force where the leaf lands, when the leaf is shaped; a
  // value that names its own sheet keeps it.
  Text e = textLeaf();
  detail::TextData& text = e.node()->textData.ensure();
  // A rich text started with no base is set in the font in force where it
  // lands, exactly as a plain leaf that names no style is; one started
  // with a base is set in that base alone.
  text.inherits = !spans.hasBase();
  // The base rides along as `style` because everything downstream that asks
  // a text leaf what it is set in — the strut a line height comes from, the
  // metric band an own-box ink maps into — reads one style, and a mixed
  // paragraph's answer to that question is the style its unstyled runs use.
  text.style = spans.base();
  text.rich = std::move(spans);
  return e;
}

Text frame(sigil::weave::Story story) {
  Text e = text(story.content());
  const std::span<const sigil::weave::ParagraphStyle> blocks = story.blocks();
  if (!blocks.empty())
    e.paragraphStyles(std::vector<sigil::weave::ParagraphStyle>(blocks.begin(),
                                                                blocks.end()));
  return e;
}

Text text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
          sigil::weave::ParagraphLayoutOptions options) {
  Text e = textLeaf();
  detail::TextData& text = e.node()->textData.ensure();
  text.paragraphOverride = std::move(paragraph);
  text.layoutOptions = std::move(options);
  return e;
}

Image image(std::shared_ptr<const sigil::image::ImageAsset> asset) {
  Image e{std::make_shared<detail::ElementNode>()};
  e.node()->kind = Kind::Image;
  e.node()->imageData.ensure().asset = std::move(asset);
  return e;
}

Image image(sk_sp<SkImage> picture, material::skia::Fit fit) {
  if (!picture) return image(std::shared_ptr<const sigil::image::ImageAsset>());
  const float w = (float)picture->width();
  const float h = (float)picture->height();
  Image leaf = image(std::make_shared<const sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(std::move(picture))));
  // Native asks nothing of the box, and the leaf already measures the
  // picture's own pixels.
  if (fit == material::skia::Fit::Native) return leaf;
  // THE FIT IS LAYOUT AND NOT A MATRIX: the leaf takes the box it stands
  // in, and where its proportions are kept the node itself is the right
  // shape — so what is painted is the whole of the node and a caller
  // computes nothing.
  if (fit == material::skia::Fit::Stretch || w <= 0.0f || h <= 0.0f)
    return leaf.width(pct(100)).height(pct(100));
  leaf.aspectRatio(w / h);
  if (fit == material::skia::Fit::Contain)
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

Element slot(std::string_view name) {
  Element e;
  e.node()->kind = Kind::Slot;
  e.node()->key = std::string(name);
  return e;
}

Element point() {
  Element e;
  // Out of the flow, so it takes no room beside its siblings, and placed
  // by its insets or a pin exactly as any absolute node is. Zero by zero,
  // so a centre pin lands it on the point it names. Out of hit testing,
  // because a point has no box to be hit in. These are the factory's
  // starting values, not statements: a rule that sizes or places a point
  // stands over them, as it could not over the point's own verbs.
  detail::LayoutProps& layout = e.node()->fields.defaults().layout;
  layout.absolute = true;
  layout.width = Dimension(0.0f);
  layout.height = Dimension(0.0f);
  e.node()->hitTestable = false;
  return e;
}

namespace detail {
Element makeLayout(Operator scheme) {
  Element e;
  e.node()->operatorData.ensure().operators.push_back(std::move(scheme));
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
