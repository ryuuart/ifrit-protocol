/** @file
 * A property by name, and a property by where its value is kept: the two
 * switches that have one line per `Property`, which is what makes adding
 * one a decision the compiler asks about rather than a silent omission.
 * With the fold that reads them, and the comparison that says whether a
 * second fold moved anything.
 */

#include "ComposeCompare.h"
#include "ComputedStyle.h"

namespace sigil::compose {

std::string_view propertyName(Property property) {
  switch (property) {
    case Property::Display:
      return "display";
    case Property::BoxSizing:
      return "boxSizing";
    case Property::Gap:
      return "gap";
    case Property::PaddingTop:
      return "paddingTop";
    case Property::PaddingRight:
      return "paddingRight";
    case Property::PaddingBottom:
      return "paddingBottom";
    case Property::PaddingLeft:
      return "paddingLeft";
    case Property::MarginTop:
      return "marginTop";
    case Property::MarginRight:
      return "marginRight";
    case Property::MarginBottom:
      return "marginBottom";
    case Property::MarginLeft:
      return "marginLeft";
    case Property::Width:
      return "width";
    case Property::Height:
      return "height";
    case Property::MinWidth:
      return "minWidth";
    case Property::MaxWidth:
      return "maxWidth";
    case Property::MinHeight:
      return "minHeight";
    case Property::MaxHeight:
      return "maxHeight";
    case Property::AspectRatio:
      return "aspectRatio";
    case Property::FlexDirection:
      return "flexDirection";
    case Property::FlexWrap:
      return "flexWrap";
    case Property::FlexBasis:
      return "flexBasis";
    case Property::FlexGrow:
      return "flexGrow";
    case Property::FlexShrink:
      return "flexShrink";
    case Property::AlignItems:
      return "alignItems";
    case Property::AlignSelf:
      return "alignSelf";
    case Property::JustifyContent:
      return "justifyContent";
    case Property::Absolute:
      return "absolute";
    case Property::Left:
      return "left";
    case Property::Top:
      return "top";
    case Property::Right:
      return "right";
    case Property::Bottom:
      return "bottom";
    case Property::CenterAt:
      return "centerAt";
    case Property::GridCells:
      return "gridCells";
    case Property::GridArea:
      return "gridArea";
    case Property::BorderRadius:
      return "borderRadius";
    case Property::Shape:
      return "shape";
    case Property::Overflow:
      return "overflow";
    case Property::Fill:
      return "fill";
    case Property::Opacity:
      return "opacity";
    case Property::BlendMode:
      return "blendMode";
    case Property::BackgroundOrigin:
      return "backgroundOrigin";
    case Property::ZIndex:
      return "zIndex";
    case Property::TranslateX:
      return "translateX";
    case Property::TranslateY:
      return "translateY";
    case Property::Rotate:
      return "rotate";
    case Property::Scale:
      return "scale";
    case Property::ScaleX:
      return "scaleX";
    case Property::ScaleY:
      return "scaleY";
    case Property::SkewX:
      return "skewX";
    case Property::SkewY:
      return "skewY";
    case Property::TransformOrigin:
      return "transformOrigin";
    case Property::RotateX:
      return "rotateX";
    case Property::RotateY:
      return "rotateY";
    case Property::TranslateZ:
      return "translateZ";
    case Property::ScaleZ:
      return "scaleZ";
    case Property::Perspective:
      return "perspective";
    case Property::PerspectiveOrigin:
      return "perspectiveOrigin";
    case Property::TransformOriginZ:
      return "transformOriginZ";
    case Property::Preserve3d:
      return "preserve3d";
    case Property::Backface:
      return "backface";
    case Property::DecorationOutline:
      return "decorationOutline";
    case Property::Font:
      return "font";
    case Property::Block:
      return "block";
    case Property::Ink:
      return "ink";
    case Property::CustomProperties:
      return "var";
    case Property::ImageRendering:
      return "imageRendering";
    case Property::kCount:
      break;
  }
  return {};
}

namespace detail {

void copyProperty(Property property, const ComputedStyle& from,
                  ComputedStyle& into) {
  LayoutProps& layout = into.layout;
  const LayoutProps& source = from.layout;
  PaintProps& paint = into.paint;
  const PaintProps& paintSource = from.paint;
  switch (property) {
    case Property::Display:
      layout.display = source.display;
      return;
    case Property::BoxSizing:
      layout.boxSizing = source.boxSizing;
      return;
    case Property::Gap:
      layout.gap = source.gap;
      return;
    case Property::PaddingTop:
      layout.padding.top = source.padding.top;
      return;
    case Property::PaddingRight:
      layout.padding.right = source.padding.right;
      return;
    case Property::PaddingBottom:
      layout.padding.bottom = source.padding.bottom;
      return;
    case Property::PaddingLeft:
      layout.padding.left = source.padding.left;
      return;
    case Property::MarginTop:
      layout.margin.top = source.margin.top;
      return;
    case Property::MarginRight:
      layout.margin.right = source.margin.right;
      return;
    case Property::MarginBottom:
      layout.margin.bottom = source.margin.bottom;
      return;
    case Property::MarginLeft:
      layout.margin.left = source.margin.left;
      return;
    case Property::Width:
      layout.width = source.width;
      return;
    case Property::Height:
      layout.height = source.height;
      return;
    case Property::MinWidth:
      layout.minWidth = source.minWidth;
      return;
    case Property::MaxWidth:
      layout.maxWidth = source.maxWidth;
      return;
    case Property::MinHeight:
      layout.minHeight = source.minHeight;
      return;
    case Property::MaxHeight:
      layout.maxHeight = source.maxHeight;
      return;
    case Property::AspectRatio:
      layout.aspect = source.aspect;
      return;
    case Property::FlexDirection:
      layout.direction = source.direction;
      return;
    case Property::FlexWrap:
      layout.wrap = source.wrap;
      return;
    case Property::FlexBasis:
      layout.basis = source.basis;
      return;
    case Property::FlexGrow:
      layout.grow = source.grow;
      return;
    case Property::FlexShrink:
      layout.shrink = source.shrink;
      return;
    case Property::AlignItems:
      layout.alignItems = source.alignItems;
      return;
    case Property::AlignSelf:
      layout.alignSelf = source.alignSelf;
      return;
    case Property::JustifyContent:
      layout.justify = source.justify;
      return;
    case Property::Absolute:
      // The flag and the bookkeeping beside it are one statement: a node
      // that takes its placement from somewhere else takes whether it was
      // pinned at all with it.
      layout.absolute = source.absolute;
      layout.hasInsets = source.hasInsets;
      return;
    case Property::Left:
      layout.insets.left = source.insets.left;
      return;
    case Property::Top:
      layout.insets.top = source.insets.top;
      return;
    case Property::Right:
      layout.insets.right = source.insets.right;
      return;
    case Property::Bottom:
      layout.insets.bottom = source.insets.bottom;
      return;
    case Property::CenterAt:
      layout.centerAt = source.centerAt;
      return;
    case Property::GridCells:
      layout.cells = source.cells;
      return;
    case Property::BorderRadius:
      into.corners = from.corners;
      return;
    case Property::Overflow:
      into.clipContent = from.clipContent;
      return;
    case Property::Fill:
      paint.fill = paintSource.fill;
      return;
    case Property::Opacity:
      paint.opacity = paintSource.opacity;
      return;
    case Property::BlendMode:
      paint.blendMode = paintSource.blendMode;
      return;
    case Property::BackgroundOrigin:
      paint.backgroundOrigin = paintSource.backgroundOrigin;
      return;
    case Property::ZIndex:
      paint.zIndex = paintSource.zIndex;
      return;
    case Property::TranslateX:
      paint.translateX = paintSource.translateX;
      return;
    case Property::TranslateY:
      paint.translateY = paintSource.translateY;
      return;
    case Property::Rotate:
      paint.rotate = paintSource.rotate;
      return;
    case Property::Scale:
      paint.scale = paintSource.scale;
      return;
    case Property::ScaleX:
      paint.scaleX = paintSource.scaleX;
      return;
    case Property::ScaleY:
      paint.scaleY = paintSource.scaleY;
      return;
    case Property::SkewX:
      paint.skewX = paintSource.skewX;
      return;
    case Property::SkewY:
      paint.skewY = paintSource.skewY;
      return;
    case Property::TransformOrigin:
      paint.originX = paintSource.originX;
      paint.originY = paintSource.originY;
      return;
    // The properties the computed style does not carry. The five the
    // CASCADE PASS resolves are answered there, where it folds its own
    // keywords over the values it inherits. The rest of this list is
    // answered NOWHERE: `answersKeyword` says so, and a keyword written
    // about one of them is refused at the verb rather than dropped here.
    case Property::GridArea:
    case Property::Shape:
    case Property::RotateX:
    case Property::RotateY:
    case Property::TranslateZ:
    case Property::ScaleZ:
    case Property::Perspective:
    case Property::PerspectiveOrigin:
    case Property::TransformOriginZ:
    case Property::Preserve3d:
    case Property::Backface:
    case Property::DecorationOutline:
    case Property::Font:
    case Property::Block:
    case Property::Ink:
    case Property::CustomProperties:
    case Property::ImageRendering:
    case Property::kCount:
      return;
  }
}

bool computedStyleEqual(const ComputedStyle& a, const ComputedStyle& b) {
  static_assert(kFieldCount<ComputedStyle> == 4,
                "ComputedStyle gained or lost a field — compare it here "
                "too. A field left out makes a re-fold that MOVED it read "
                "as one that moved nothing, and the picture recorded from "
                "the old value replays.");
  static_assert(kFieldCount<PaintProps> == 15,
                "PaintProps gained or lost a field — compare it below.");
  if (!(a.layout == b.layout) || !(a.corners == b.corners) ||
      a.clipContent != b.clipContent)
    return false;
  const PaintProps &pa = a.paint, &pb = b.paint;
  if (pa.fill.has_value() != pb.fill.has_value()) return false;
  if (pa.fill && !propertyEqual(*pa.fill, *pb.fill)) return false;
  return propertyEqual(pa.opacity, pb.opacity) &&
         pa.blendMode == pb.blendMode &&
         pa.backgroundOrigin == pb.backgroundOrigin &&
         propertyEqual(pa.translateX, pb.translateX) &&
         propertyEqual(pa.translateY, pb.translateY) &&
         propertyEqual(pa.rotate, pb.rotate) &&
         propertyEqual(pa.scale, pb.scale) &&
         propertyEqual(pa.scaleX, pb.scaleX) &&
         propertyEqual(pa.scaleY, pb.scaleY) &&
         propertyEqual(pa.skewX, pb.skewX) &&
         propertyEqual(pa.skewY, pb.skewY) && pa.originX == pb.originX &&
         pa.originY == pb.originY && pa.zIndex == pb.zIndex;
}

void resolveStyle(const ComputedStyle* parent, const ElementNode& node,
                  ComputedStyle& out) {
  out.layout = node.layout;
  out.paint = node.paint;
  out.corners = node.corners;
  out.clipContent = node.clipContent;
  // DEFAULT INHERITANCE: a property that inherits and that this node says
  // nothing about takes the parent's computed value. The list is built
  // from `inheritsByDefault` at compile time, so that table is the whole
  // of the set. Every property on it today is one the cascade pass
  // resolves and the computed style does not carry, which makes each of
  // these calls a switch that returns at once; a property moved into the
  // set that the style DOES carry inherits here with nothing else to
  // write.
  if (parent != nullptr)
    for (const Property property : kInherited)
      if (!node.declared.has(property)) copyProperty(property, *parent, out);
  if (!node.keywords) return;
  // The initial value of every property is the value its field carries on
  // a node nobody wrote to, so one default style IS the whole table of
  // them. A root, which has no parent, inherits from the same place: there
  // is no ancestor to take a value from and the initial one is what CSS
  // gives it.
  static const ComputedStyle initial;
  for (const sigil::weave::KeywordTable<Property>::Entry& entry :
       node.keywords->entries()) {
    const sigil::weave::Keyword keyword =
        resolveKeyword(entry.keyword, entry.field);
    const ComputedStyle* from =
        keyword == sigil::weave::Keyword::Inherit && parent != nullptr
            ? parent
            : &initial;
    copyProperty(entry.field, *from, out);
  }
}

}  // namespace detail

}  // namespace sigil::compose
