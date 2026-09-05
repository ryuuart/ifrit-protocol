/** @file
 * The stock tools, the per-stroke roll of a tool's randomness, and how
 * far apart the tool lays its dabs.
 */

#include <sigildraw/Pen.h>
#include <sigildraw/brush/Tool.h>

#include <algorithm>

namespace sigil::draw::brush {

Tool pencil(SkColor4f color, float width) {
  return Tool{.tip = Tip::Dust,
              .color = color,
              .width = width,
              .spacing = std::max(0.08f, width * 0.32f),
              .opacity = 0.68f,
              .scatter = 0.6f,
              .density = 0.76f,
              .bristles = 5,
              .pressure = {0.72f, 1.0f, 0.64f},
              .sizeJitter = 0.08f,
              .opacityJitter = 0.12f,
              .spacingJitter = 0.08f,
              .noise = 0.3f};
}

Tool charcoal(SkColor4f color, float width) {
  return Tool{.tip = Tip::Dust,
              .color = color,
              .width = width,
              .spacing = std::max(0.08f, width * 0.09f),
              .opacity = 0.34f,
              .scatter = 5.5f,
              .density = 1.35f,
              .bristles = 18,
              .pressure = {0.55f, 1.0f, 0.48f},
              .blend = MULTIPLY,
              .sizeJitter = 0.14f,
              .opacityJitter = 0.18f,
              .spacingJitter = 0.16f,
              .noise = 0.45f};
}

Tool marker(SkColor4f color, float width) {
  return Tool{.tip = Tip::Nib,
              .color = color,
              .width = width,
              .spacing = std::max(0.04f, width * 0.02f),
              .opacity = 0.46f,
              .scatter = width * 0.10f,
              .density = 1.0f,
              .bristles = 2,
              .pressure = {0.72f, 1.05f, 0.54f},
              .sizeJitter = 0.10f,
              .opacityJitter = 0.10f,
              .spacingJitter = 0.12f,
              .noise = 0.3f};
}

Tool watercolor(SkColor4f color, float width) {
  return Tool{.tip = Tip::Fibres,
              .color = color,
              .width = width,
              .spacing = 1.1f,
              .opacity = 0.11f,
              .scatter = 10.0f,
              .density = 0.86f,
              .bristles = 34,
              .pressure = {0.35f, 1.0f, 0.22f},
              .blend = MULTIPLY};
}

Tool spray(SkColor4f color, float width) {
  return Tool{.tip = Tip::Scatter,
              .color = color,
              .width = width * 0.12f,
              .spacing = 2.2f,
              .opacity = 0.38f,
              .scatter = width,
              .density = 0.82f,
              .bristles = 12,
              .pressure = {0.5f, 1.0f, 0.38f}};
}

Tool prepareStroke(Pen& pen, const Tool& tool) {
  Tool rolled = tool;
  if (rolled.pressure.gaussian) {
    Pressure::Gaussian& profile = *rolled.pressure.gaussian;
    profile.center += pen.random(-profile.centerJitter, profile.centerJitter);
    profile.width *= 1.0f - profile.widthJitter * pen.random(1.0f, 1.5f);
    profile.width = std::max(0.025f, profile.width);
  } else if (rolled.pressure.variation) {
    const Pressure source = rolled.pressure;
    const Pressure::Variation variation = *source.variation;
    const float offset = pen.random(-variation.offset, variation.offset);
    const float scale =
        pen.random(1.0f - variation.scale, 1.0f + variation.scale);
    const float warp = pen.random(-variation.warp, variation.warp);
    const float tilt = pen.random(-variation.tilt, variation.tilt);
    rolled.pressure.curve = [source, offset, scale, warp,
                             tilt](float progress) {
      const float at =
          std::clamp(0.5f + (progress - 0.5f + warp) * scale, 0.0f, 1.0f);
      return source.at(at) + offset + tilt * (progress - 0.5f);
    };
    rolled.pressure.variation.reset();
  }
  const float noise = std::clamp(rolled.noise, 0.0f, 1.0f);
  if (noise > 0.0f)
    rolled.opacity = std::max(
        0.0f, rolled.opacity * (1.0f + pen.randomGaussian(0.0f, noise * 0.1f)));
  return rolled;
}

float spacingOf(const Tool& tool) {
  if (!tool.shape) return tool.spacing;
  return std::max(0.05f, tool.shape->spacing * tool.width);
}

}  // namespace sigil::draw::brush
