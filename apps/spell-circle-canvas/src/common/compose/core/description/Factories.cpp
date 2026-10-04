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
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Image.h>

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

Element scene() {
  Element e;
  e.node()->kind = Kind::Scene;
  return e;
}

Element light(material::Light source) {
  Element e;
  e.node()->kind = Kind::Light;
  e.node()->sceneData.ensure().source = std::move(source);
  detail::LayoutProps& layout =
      e.node()->fields.defaults(detail::DefaultsKey{}).layout;
  layout.absolute = true;
  layout.width = Dimension(0.0f);
  layout.height = Dimension(0.0f);
  e.node()->hitTestable = false;
  return e;
}

Element stack() {
  Element e;
  e.node()->kind = Kind::Stack;
  return e;
}

Element positioned() {
  Element e;
  // What kind of container the node is, not a property it states.
  e.node()->fields.defaults(detail::DefaultsKey{}).layout.positioned = true;
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

Image image(sigil::media::PixelSource source, material::Fit fit) {
  const glm::ivec2 size = source.size();
  Image leaf{std::make_shared<detail::ElementNode>()};
  leaf.node()->kind = Kind::Image;
  leaf.node()->imageData.ensure().source = std::move(source);
  // Native asks nothing of the box.
  if (fit == material::Fit::Native) return leaf;
  // THE FIT IS LAYOUT AND NOT A MATRIX: the leaf takes the box it stands
  // in, and where its proportions are kept the node itself is the right
  // shape — so what is painted is the whole of the node and a caller
  // computes nothing.
  const float width = (float)size.x;
  const float height = (float)size.y;
  if (fit == material::Fit::Stretch || width <= 0.0f || height <= 0.0f)
    return leaf.width(pct(100)).height(pct(100));
  leaf.aspectRatio(width / height);
  if (fit == material::Fit::Contain)
    leaf.width(pct(100)).maxWidth(pct(100)).maxHeight(pct(100));
  else
    leaf.height(pct(100)).minWidth(pct(100));
  return leaf;
}

Image image(sk_sp<SkImage> picture, material::Fit fit) {
  // A picture that is not there takes no room, whatever the fit.
  if (!picture)
    return image(sigil::media::PixelSource(), material::Fit::Native);
  return image(sigil::media::PixelSource(std::move(picture)), fit);
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
                         draw::Pen& pen, const PaintContext& ctx) {
                       SkCanvas& canvas = *pen.canvas();
                       SkAutoCanvasRestore restore(&canvas, true);
                       if (recordedAt.width() > 0 && recordedAt.height() > 0)
                         canvas.scale(ctx.size.x / recordedAt.width(),
                                      ctx.size.y / recordedAt.height());
                       canvas.drawPicture(pic.get());
                     });
  return e.width(Dimension(native.width())).height(Dimension(native.height()));
}

Element pathFigure(const geometry::path::Outline& absolute, float bleed) {
  // The box is the path's control-point bounds, as Skia measures them, so
  // a figure keeps the room its curves' handles reach toward.
  SkRect bounds = geometry::path::toSk(absolute).getBounds();
  bounds.outset(bleed, bleed);
  const geometry::path::Rect box = geometry::path::fromSk(bounds);
  return compose::box().absolute().rect(box).shape(heldPath(
      absolute.transformed(geometry::path::Transform::translate(-box.min))));
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
  detail::LayoutProps& layout =
      e.node()->fields.defaults(detail::DefaultsKey{}).layout;
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
