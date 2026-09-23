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
    case Property::GridCellAlign:
      return "gridCellAlign";
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
    case Property::Paragraph:
      return "paragraph";
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
      // Which cells, apart from where in them: CSS keeps placement and
      // self-alignment as two properties, so a rule's alignment stands
      // beside the element's own placement.
      layout.cells.column = source.cells.column;
      layout.cells.row = source.cells.row;
      layout.cells.columns = source.cells.columns;
      layout.cells.rows = source.cells.rows;
      layout.cells.declared = source.cells.declared;
      return;
    case Property::GridCellAlign:
      layout.cells.across = source.cells.across;
      layout.cells.down = source.cells.down;
      layout.cells.alignDeclared = source.cells.alignDeclared;
      return;
    case Property::BorderRadius:
      into.corners = from.corners;
      return;
    case Property::Overflow:
      into.clipContent = from.clipContent;
      return;
    case Property::Fill:
      paint.fill = paintSource.fill;
      paint.fillBox = paintSource.fillBox;
      return;
    case Property::Opacity:
      paint.opacity = paintSource.opacity;
      return;
    case Property::BlendMode:
      paint.blendMode = paintSource.blendMode;
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
    case Property::Paragraph:
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
         pa.blendMode == pb.blendMode && pa.fillBox == pb.fillBox &&
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

namespace {

/** Whether a rule may state @p property into the layer: the computed
 *  style carries it, and the cascade pass does not answer it elsewhere. */
constexpr bool carriedByLayer(Property property) {
  return answersKeyword(property) && !inheritsByDefault(property);
}

/** The value a property holds on @p node where that value is one a rule
 *  can hold — anything but a live binding or an animation, which stay
 *  verbs on the element. */
template <class T>
bool staticValue(const motion::Animatable<T>& value) {
  return value.plain() != nullptr;
}

bool ruleCanHold(Property property, const ElementNode& node) {
  const PaintProps& paint = node.fields.paint();
  switch (property) {
    case Property::Fill:
      // A paint resolved against the box it lands on travels in the
      // layer's material slot; one that animates is a live form.
      if (node.materialData && node.materialData->live &&
          node.materialData->live->isAnimated())
        return false;
      return !paint.fill || staticValue(*paint.fill);
    case Property::Opacity:
      return staticValue(paint.opacity);
    case Property::TranslateX:
      return staticValue(paint.translateX);
    case Property::TranslateY:
      return staticValue(paint.translateY);
    case Property::Rotate:
      return staticValue(paint.rotate);
    case Property::Scale:
      return staticValue(paint.scale);
    case Property::ScaleX:
      return staticValue(paint.scaleX);
    case Property::ScaleY:
      return staticValue(paint.scaleY);
    case Property::SkewX:
      return staticValue(paint.skewX);
    case Property::SkewY:
      return staticValue(paint.skewY);
    default:
      return true;
  }
}

}  // namespace

std::unique_ptr<const RuleLayer> ruleLayerOf(
    std::span<const Rule* const> matched) {
  std::unique_ptr<RuleLayer> layer;
  for (const Rule* rule : matched) {
    const ElementNode& stated = *rule->node();
    if (stated.fields.declared().empty()) continue;
    // The rule's own declarations, verbatim: the same fields a node's
    // style is filled from, so one row of `copyProperty` moves a property
    // out of either.
    ComputedStyle values;
    bool filled = false;
    for (size_t i = 0; i < (size_t)Property::kCount; ++i) {
      const auto property = (Property)i;
      if (!stated.fields.declared().has(property) || !carriedByLayer(property))
        continue;
      if (!layer) layer = std::make_unique<RuleLayer>();
      if (const std::optional<sigil::weave::Keyword> keyword =
              stated.fields.keywords()
                  ? stated.fields.keywords()->find(property)
                  : std::nullopt) {
        layer->keywords.set(property, *keyword);
        layer->declared.set(property);
        if (property == Property::Fill) layer->material.reset();
        continue;
      }
      if (!ruleCanHold(property, stated)) {
        warnRuleHoldsOnlyStaticValues(property);
        continue;
      }
      if (!filled) {
        values.layout = stated.fields.layout();
        values.paint = stated.fields.paint();
        values.corners = stated.fields.corners();
        values.clipContent = stated.fields.clipContent();
        filled = true;
      }
      copyProperty(property, values, layer->values);
      // The fill's paint goes with it, as the node's own slot keeps it,
      // and a stronger rule's plain fill leaves none behind.
      if (property == Property::Fill) {
        if (stated.materialData)
          layer->material = *stated.materialData;
        else
          layer->material.reset();
      }
      layer->keywords.clear(property);
      layer->declared.set(property);
    }
  }
  return layer;
}

namespace {

bool materialSlotEqual(const std::optional<MaterialData>& a,
                       const std::optional<MaterialData>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || (a->live == b->live && a->recipe == b->recipe);
}

}  // namespace

bool ruleFillMaterialEqual(const RuleLayer* a, const RuleLayer* b) {
  static const std::optional<MaterialData> none;
  return materialSlotEqual(a != nullptr ? a->material : none,
                           b != nullptr ? b->material : none);
}

bool ruleLayerEqual(const RuleLayer* a, const RuleLayer* b) {
  if (a == nullptr || b == nullptr) return a == b;
  return a->declared == b->declared && a->keywords == b->keywords &&
         computedStyleEqual(a->values, b->values) &&
         materialSlotEqual(a->material, b->material);
}

void resolveStyle(const ComputedStyle* parent, const ElementNode& node,
                  ComputedStyle& out, const RuleLayer* rules) {
  out.layout = node.fields.layout();
  out.paint = node.fields.paint();
  out.corners = node.fields.corners();
  out.clipContent = node.fields.clipContent();
  // THE MATCHED RULES, under the node's own verbs: a property the node
  // states stands, and one it leaves unsaid takes the strongest rule's
  // statement of it.
  if (rules != nullptr)
    for (size_t i = 0; i < (size_t)Property::kCount; ++i) {
      const auto property = (Property)i;
      if (rules->declared.has(property) &&
          !node.fields.declared().has(property) &&
          !rules->keywords.find(property))
        copyProperty(property, rules->values, out);
    }
  // DEFAULT INHERITANCE: a property that inherits and that neither this
  // node nor a rule says anything about takes the parent's computed value.
  // The list is built from `inheritsByDefault` at compile time, so that
  // table is the whole of the set. Every property on it today is one the
  // cascade pass resolves and the computed style does not carry, which
  // makes each of these calls a switch that returns at once; a property
  // moved into the set that the style DOES carry inherits here with
  // nothing else to write.
  if (parent != nullptr)
    for (const Property property : kInherited)
      if (!node.fields.declared().has(property) &&
          (rules == nullptr || !rules->declared.has(property)))
        copyProperty(property, *parent, out);
  const bool ruleKeywords = rules != nullptr && !rules->keywords.empty();
  if (!node.fields.keywords() && !ruleKeywords) return;
  // The initial value of every property is the value its field carries on
  // a node nobody wrote to, so one default style IS the whole table of
  // them. A root, which has no parent, inherits from the same place: there
  // is no ancestor to take a value from and the initial one is what CSS
  // gives it.
  static const ComputedStyle initial;
  const auto resolve =
      [&](const sigil::weave::KeywordTable<Property>::Entry& entry) {
        const sigil::weave::Keyword keyword =
            resolveKeyword(entry.keyword, entry.field);
        const ComputedStyle* from =
            keyword == sigil::weave::Keyword::Inherit && parent != nullptr
                ? parent
                : &initial;
        copyProperty(entry.field, *from, out);
      };
  // A rule's keyword stands where the node states nothing; the node's
  // own keyword is its own statement and stands over every rule.
  if (ruleKeywords)
    for (const auto& entry : rules->keywords.entries())
      if (!node.fields.declared().has(entry.field)) resolve(entry);
  if (node.fields.keywords())
    for (const auto& entry : node.fields.keywords()->entries()) resolve(entry);
}

}  // namespace detail

}  // namespace sigil::compose
