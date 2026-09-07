/** @file
 * p5's colour arguments: the models and their ranges, and the CSS string.
 */

#include <sigildraw/Color.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace sigil::draw {

namespace {

float unit(float v, float max) {
  if (!(max > 0.0f)) return 0.0f;
  return std::clamp(v / max, 0.0f, 1.0f);
}

/** Hue in [0, 1), saturation and value in [0, 1] to RGB. */
SkColor4f fromHsb(float h, float s, float v, float a) {
  h = h - std::floor(h);
  const float c = v * s;
  const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
  const float m = v - c;
  float r = 0, g = 0, b = 0;
  const int sector = (int)std::floor(h * 6.0f) % 6;
  switch (sector) {
    case 0:
      r = c;
      g = x;
      break;
    case 1:
      r = x;
      g = c;
      break;
    case 2:
      g = c;
      b = x;
      break;
    case 3:
      g = x;
      b = c;
      break;
    case 4:
      r = x;
      b = c;
      break;
    default:
      r = c;
      b = x;
      break;
  }
  return {r + m, g + m, b + m, a};
}

/** Hue in [0, 1), saturation and lightness in [0, 1] to RGB. */
SkColor4f fromHsl(float h, float s, float l, float a) {
  const float c = (1.0f - std::fabs(2.0f * l - 1.0f)) * s;
  const float v = l + c / 2.0f;
  const float sv = v > 0.0f ? c / v : 0.0f;
  return fromHsb(h, sv, v, a);
}

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

struct Named {
  std::string_view name;
  uint32_t rgb;
};

constexpr Named kNamed[] = {
    {"black", 0x000000},    {"white", 0xffffff},     {"red", 0xff0000},
    {"lime", 0x00ff00},     {"blue", 0x0000ff},      {"yellow", 0xffff00},
    {"cyan", 0x00ffff},     {"aqua", 0x00ffff},      {"magenta", 0xff00ff},
    {"fuchsia", 0xff00ff},  {"silver", 0xc0c0c0},    {"gray", 0x808080},
    {"grey", 0x808080},     {"maroon", 0x800000},    {"olive", 0x808000},
    {"green", 0x008000},    {"purple", 0x800080},    {"teal", 0x008080},
    {"navy", 0x000080},     {"orange", 0xffa500},    {"pink", 0xffc0cb},
    {"brown", 0xa52a2a},    {"gold", 0xffd700},      {"violet", 0xee82ee},
    {"indigo", 0x4b0082},   {"crimson", 0xdc143c},   {"coral", 0xff7f50},
    {"salmon", 0xfa8072},   {"tomato", 0xff6347},    {"turquoise", 0x40e0d0},
    {"skyblue", 0x87ceeb},  {"steelblue", 0x4682b4}, {"slategray", 0x708090},
    {"darkgray", 0xa9a9a9}, {"lightgray", 0xd3d3d3},
};

}  // namespace

ColorMode ColorMode::standard(Constant mode) {
  if (mode == HSB || mode == HSL) return {mode, 360.0f, 100.0f, 100.0f, 1.0f};
  return {RGB, 255.0f, 255.0f, 255.0f, 255.0f};
}

SkColor4f colorFrom(const ColorMode& mode, float v1, float v2, float v3,
                    float alpha) {
  const float a = unit(alpha, mode.maxA);
  const float c2 = unit(v2, mode.max2);
  const float c3 = unit(v3, mode.max3);
  switch (mode.mode) {
    case HSB:
      return fromHsb(unit(v1, mode.max1), c2, c3, a);
    case HSL:
      return fromHsl(unit(v1, mode.max1), c2, c3, a);
    default:
      return {unit(v1, mode.max1), c2, c3, a};
  }
}

SkColor4f colorFrom(const ColorMode& mode, float gray, float alpha) {
  const float g = unit(gray, mode.max3);
  return {g, g, g, unit(alpha, mode.maxA)};
}

SkColor4f parseColor(std::string_view css) {
  if (!css.empty() && css.front() == '#') {
    const std::string_view hex = css.substr(1);
    int digits[8];
    if (hex.size() != 3 && hex.size() != 4 && hex.size() != 6 &&
        hex.size() != 8)
      return {0, 0, 0, 1};
    for (size_t i = 0; i < hex.size(); ++i) {
      digits[i] = hexDigit(hex[i]);
      if (digits[i] < 0) return {0, 0, 0, 1};
    }
    float channel[4] = {0, 0, 0, 1};
    if (hex.size() <= 4) {
      for (size_t i = 0; i < hex.size(); ++i)
        channel[i] = (float)(digits[i] * 17) / 255.0f;
    } else {
      for (size_t i = 0; i < hex.size() / 2; ++i)
        channel[i] = (float)(digits[2 * i] * 16 + digits[2 * i + 1]) / 255.0f;
    }
    return {channel[0], channel[1], channel[2], channel[3]};
  }
  if (css == "transparent") return {0, 0, 0, 0};
  for (const Named& named : kNamed)
    if (named.name == css)
      return {(float)((named.rgb >> 16u) & 0xffu) / 255.0f,
              (float)((named.rgb >> 8u) & 0xffu) / 255.0f,
              (float)(named.rgb & 0xffu) / 255.0f, 1.0f};
  return {0, 0, 0, 1};
}

}  // namespace sigil::draw
