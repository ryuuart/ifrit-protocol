/** @file
 * The walk around the hue wheel, and a colour read from its CSS text.
 */

#include "sigilmaterial/color/Color.h"

namespace sigil::material {

namespace {

int hexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

struct NamedColor {
  std::string_view name;
  uint32_t rrggbb;
};

constexpr NamedColor kNamedColors[] = {
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

constexpr Color kUnreadable{0, 0, 0, 1};

}  // namespace

Color parseColor(std::string_view css) {
  if (!css.empty() && css.front() == '#') {
    const std::string_view hex = css.substr(1);
    if (hex.size() != 3 && hex.size() != 4 && hex.size() != 6 &&
        hex.size() != 8)
      return kUnreadable;
    int digits[8];
    for (size_t index = 0; index < hex.size(); ++index) {
      digits[index] = hexDigit(hex[index]);
      if (digits[index] < 0) return kUnreadable;
    }
    float channels[4] = {0, 0, 0, 1};
    if (hex.size() <= 4) {
      // One digit stands for itself twice, so `f` is `ff` and 17 times it.
      for (size_t index = 0; index < hex.size(); ++index)
        channels[index] = (float)(digits[index] * 17) / 255.0f;
    } else {
      for (size_t index = 0; index < hex.size() / 2; ++index)
        channels[index] =
            (float)(digits[2 * index] * 16 + digits[2 * index + 1]) / 255.0f;
    }
    return {channels[0], channels[1], channels[2], channels[3]};
  }
  if (css == "transparent") return {0, 0, 0, 0};
  for (const NamedColor& named : kNamedColors)
    if (named.name == css) return hexColor(named.rrggbb);
  return kUnreadable;
}

Color hsv(float hueDegrees, float saturation, float value, float a) {
  float h = std::fmod(hueDegrees, 360.0f);
  if (h < 0.0f) h += 360.0f;
  const float s = std::clamp(saturation, 0.0f, 1.0f);
  const float v = std::clamp(value, 0.0f, 1.0f);
  const float chroma = v * s;
  const float sector = h / 60.0f;
  // The ramp across one sixth of the wheel: full at the two primaries
  // that bound the sector, zero at the secondary between them.
  const float ramp = chroma * (1.0f - std::abs(std::fmod(sector, 2.0f) - 1.0f));
  const float base = v - chroma;
  float r = 0, g = 0, b = 0;
  switch ((int)sector) {
    case 0:
      r = chroma, g = ramp;
      break;
    case 1:
      r = ramp, g = chroma;
      break;
    case 2:
      g = chroma, b = ramp;
      break;
    case 3:
      g = ramp, b = chroma;
      break;
    case 4:
      r = ramp, b = chroma;
      break;
    default:
      r = chroma, b = ramp;
      break;
  }
  return {r + base, g + base, b + base, a};
}

}  // namespace sigil::material
