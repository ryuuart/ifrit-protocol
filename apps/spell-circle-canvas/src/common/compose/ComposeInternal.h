#pragma once
// Internal to SigilCompose — the element description payload shared by
// the builders (Compose.cpp) and the reconciler (Composer.cpp).

#include "sigilcompose/Compose.h"

#include <sigilweave/Paragraph.h>

namespace sigil::compose::detail {

enum class Kind : uint8_t { Box, Stack, Text, Image, Custom, Slot };

struct EdgeValues {
  float left = 0, top = 0, right = 0, bottom = 0;
  bool operator==(const EdgeValues &) const = default;
};

struct LayoutProps {
  bool row = false;
  float gap = 0;
  EdgeValues padding, margin;
  Dim width, height, minWidth, maxWidth, minHeight, maxHeight, basis;
  float aspect = 0;
  float grow = 0, shrink = 1;
  Align alignItems = Align::Stretch;
  Align alignSelf = Align::Auto;
  Justify justify = Justify::Start;
  bool absolute = false;
  bool hasInsets = false;
  EdgeValues insets;
  bool operator==(const LayoutProps &) const = default;
};

struct PaintProps {
  std::optional<PropValue<Fill>> fill;
  PropValue<float> opacity = 1.0f;
  SkBlendMode blendMode = SkBlendMode::kSrcOver;
  PropValue<float> translateX = 0.0f, translateY = 0.0f;
  PropValue<float> rotate = 0.0f, scale = 1.0f;
  float originX = 0.5f, originY = 0.5f;
  int zIndex = 0;
};

struct ElementNode {
  Kind kind = Kind::Box;
  std::string key;
  LayoutProps layout;
  PaintProps paint;
  Corners corners;
  std::function<SkPath(SkSize)> shapeFn; // custom outline; overrides corners
  bool clipContent = false;
  Cache cacheMode = Cache::Auto;
  std::optional<Transition> nodeTransition;

  // Text
  std::u8string textUtf8;
  sigil::weave::TextStyle textStyle;

  // Image
  std::shared_ptr<const sigil::image::ImageAsset> imageAsset;

  // Custom
  PaintProgram program;

  // Decoration layers (kernel seam; primitives live in Decorations.h)
  std::vector<Decoration> backgrounds;
  std::vector<Decoration> foregrounds;

  // Layer effects
  std::optional<Effect> layerEffect;
  std::optional<Effect> backdropEffect;

  // Custom layout (layout() containers)
  std::function<std::vector<SkRect>(const LayoutInput &)> placeFn;

  // Derive phase
  std::vector<std::string> flowAroundKeys;
  float flowAroundMargin = 0;
  std::string connectFrom, connectTo;
  Router router;

  // Memo (deferred description)
  bool isMemo = false;
  std::any memoProps;
  std::function<bool(const std::any &, const std::any &)> memoEqual;
  std::function<Element(const std::any &)> memoInvoke;

  std::vector<Element> children;
};

/** Constant, binding, or transitioned — flattened for the reconciler. */
template <typename T> struct ResolvedProp {
  T target{};
  const choreograph::Output<T> *binding = nullptr;
  const Transition *transition = nullptr; // from with() or node default
};

template <typename T>
ResolvedProp<T> resolveProp(const PropValue<T> &v,
                            const std::optional<Transition> &nodeDefault) {
  ResolvedProp<T> out;
  if (const T *plain = std::get_if<T>(&v)) {
    out.target = *plain;
    if (nodeDefault)
      out.transition = &*nodeDefault;
  } else if (const Transitioned<T> *tr = std::get_if<Transitioned<T>>(&v)) {
    out.target = tr->value;
    out.transition = &tr->spec;
  } else {
    out.binding = std::get<const choreograph::Output<T> *>(v);
  }
  return out;
}

} // namespace sigil::compose::detail
