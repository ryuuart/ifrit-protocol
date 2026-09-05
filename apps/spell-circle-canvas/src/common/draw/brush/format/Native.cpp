/** @file
 * The native brush: a JSON description beside its two images, and the
 * sniff that sends any other bytes to the importer that knows them.
 */

#include "Images.h"
#include "Zip.h"

#include <sigildraw/brush/format/Load.h>
#include <sigildraw/brush/format/Photoshop.h>
#include <sigildraw/brush/format/Procreate.h>

#include <simdjson.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace sigil::draw::brush::format {

namespace {

/** The words the description spells each enumeration with, in the
 *  declaration order of the enumeration itself, so a name and a value
 *  are one table. */
constexpr std::string_view kMaskNames[] = {"invertedLuminance", "alpha"};
constexpr std::string_view kSpaceNames[] = {"stroke", "dab"};
constexpr std::string_view kRotationNames[] = {"fixed", "natural", "random",
                                               "tilt"};
constexpr std::string_view kTipNames[] = {"dust",    "fibres", "nib",
                                          "scatter", "image",  "custom"};
constexpr std::string_view kDriveNames[] = {"pressure", "velocity", "tilt"};

template <typename Enumeration, size_t Count>
Enumeration named(std::string_view word,
                  const std::string_view (&names)[Count],
                  Enumeration fallback) {
  for (size_t index = 0; index < Count; ++index)
    if (names[index] == word) return (Enumeration)index;
  return fallback;
}

template <typename Enumeration, size_t Count>
std::string_view wordFor(Enumeration value,
                         const std::string_view (&names)[Count]) {
  const size_t index = (size_t)value;
  return index < Count ? names[index] : names[0];
}

float number(simdjson::dom::object object, std::string_view key,
             float fallback) {
  double value = 0;
  if (object[key].get_double().get(value)) return fallback;
  return (float)value;
}

std::string_view word(simdjson::dom::object object, std::string_view key,
                      std::string_view fallback) {
  std::string_view value;
  if (object[key].get_string().get(value)) return fallback;
  return value;
}

std::optional<Curve> curveFrom(simdjson::dom::object object) {
  Curve curve;
  curve.minimum = number(object, "minimum", curve.minimum);
  curve.maximum = number(object, "maximum", curve.maximum);
  curve.bend = number(object, "bend", curve.bend);
  return curve;
}

std::optional<Response> responseFrom(simdjson::dom::element element) {
  simdjson::dom::object object;
  if (element.get_object().get(object)) return std::nullopt;
  Response response;
  response.drive =
      named(word(object, "drive", "pressure"), kDriveNames, Drive::Pressure);
  if (std::optional<Curve> curve = curveFrom(object))
    response.curve = std::move(*curve);
  return response;
}

/** A float written the way the reader parses it back: enough digits to
 *  survive a round trip, and no exponent form for the ordinary range. */
std::string decimal(float value) {
  char text[32];
  std::snprintf(text, sizeof(text), "%.6g", (double)value);
  return text;
}

void writeResponse(std::string& out, std::string_view name,
                   const Response& response) {
  out += "    \"";
  out += name;
  out += "\": {\"drive\": \"";
  out += wordFor(response.drive, kDriveNames);
  out += "\", \"minimum\": " + decimal(response.curve.minimum);
  out += ", \"maximum\": " + decimal(response.curve.maximum);
  out += ", \"bend\": " + decimal(response.curve.bend) + "}";
}

/** Reads the description over @p tool, leaving every key it does not
 *  name alone. */
void readDescription(std::span<const std::byte> description, Tool& tool) {
  if (description.empty()) return;
  simdjson::dom::parser parser;
  simdjson::dom::element document;
  const std::string_view text((const char*)description.data(),
                              description.size());
  if (parser.parse(simdjson::padded_string(text)).get(document)) return;
  simdjson::dom::object root;
  if (document.get_object().get(root)) return;

  tool.tip = named(word(root, "tip", "image"), kTipNames, Tip::Image);
  tool.width = number(root, "width", tool.width);
  tool.opacity = number(root, "opacity", tool.opacity);
  tool.spacing = number(root, "spacing", tool.spacing);
  tool.scatter = number(root, "scatter", tool.scatter);
  tool.density = number(root, "density", tool.density);
  tool.angle = number(root, "angle", tool.angle);
  tool.aspect = number(root, "aspect", tool.aspect);
  tool.sizeJitter = number(root, "sizeJitter", tool.sizeJitter);
  tool.opacityJitter = number(root, "opacityJitter", tool.opacityJitter);
  tool.spacingJitter = number(root, "spacingJitter", tool.spacingJitter);
  tool.rotation = named(word(root, "rotation", "natural"), kRotationNames,
                        Rotation::Natural);

  simdjson::dom::array color;
  if (!root["color"].get_array().get(color) && color.size() >= 3) {
    float channels[4] = {0, 0, 0, 1};
    size_t index = 0;
    for (simdjson::dom::element value : color) {
      double component = 0;
      if (index < 4 && !value.get_double().get(component))
        channels[index] = (float)component;
      ++index;
    }
    tool.color = {channels[0], channels[1], channels[2], channels[3]};
  }

  simdjson::dom::object shape;
  if (!root["shape"].get_object().get(shape)) {
    Shape source = tool.shape.value_or(Shape{});
    source.mask = named(word(shape, "mask", "invertedLuminance"), kMaskNames,
                        source.mask);
    source.spacing = number(shape, "spacing", source.spacing);
    source.scatter = number(shape, "scatter", source.scatter);
    source.angleJitter = number(shape, "angleJitter", source.angleJitter);
    tool.shape = std::move(source);
  }

  simdjson::dom::object grain;
  if (!root["grain"].get_object().get(grain)) {
    Grain source = tool.grain.value_or(Grain{});
    source.space =
        named(word(grain, "space", "stroke"), kSpaceNames, source.space);
    source.scale = number(grain, "scale", source.scale);
    source.depth = number(grain, "depth", source.depth);
    tool.grain = std::move(source);
  }

  simdjson::dom::object dynamics;
  if (!root["dynamics"].get_object().get(dynamics)) {
    simdjson::dom::element response;
    if (!dynamics["size"].get(response))
      tool.dynamics.size = responseFrom(response);
    if (!dynamics["opacity"].get(response))
      tool.dynamics.opacity = responseFrom(response);
    if (!dynamics["flow"].get(response))
      tool.dynamics.flow = responseFrom(response);
  }
}

}  // namespace

std::optional<Tool> assembleBrush(std::span<const std::byte> description,
                                  std::span<const std::byte> shape,
                                  std::span<const std::byte> grain) {
  if (description.empty() && shape.empty() && grain.empty())
    return std::nullopt;

  Tool tool;
  tool.tip = Tip::Image;
  tool.opacity = 1.0f;
  tool.markerTip = false;
  tool.pressure = {1.0f, 1.0f, 1.0f};
  tool.pressure.variation.reset();

  if (sk_sp<SkImage> artwork = decodeArtwork(shape))
    tool.shape = Shape{.image = std::move(artwork)};
  if (sk_sp<SkImage> texture = decodeArtwork(grain))
    tool.grain = Grain{.image = std::move(texture)};
  readDescription(description, tool);

  // A description that names a shape's numbers without an image beside
  // it is a description of nothing to stamp: the tip falls back to the
  // procedural nib the numbers still make sense for.
  if (!tool.shape || !tool.shape->image) {
    tool.shape.reset();
    if (tool.tip == Tip::Image) tool.tip = Tip::Nib;
  }
  if (tool.grain && !tool.grain->image) tool.grain.reset();
  return tool;
}

std::optional<Tool> decodeBrush(const io::Bytes& bytes, std::string_view hint) {
  const std::span<const std::byte> all(bytes.bytes);
  if (all.empty()) return std::nullopt;

  if (isZip(all)) {
    const std::vector<ZipEntry> entries = readZip(all);
    std::span<const std::byte> description;
    std::span<const std::byte> shape;
    std::span<const std::byte> grain;
    for (const ZipEntry& entry : entries) {
      const std::string_view name(entry.name);
      if (name.ends_with(kDescriptionName))
        description = entry.bytes;
      else if (name.ends_with(kShapeName))
        shape = entry.bytes;
      else if (name.ends_with(kGrainName))
        grain = entry.bytes;
    }
    if (!description.empty() || !shape.empty())
      return assembleBrush(description, shape, grain);
    return decodeProcreateBrush(all);
  }

  if (isPhotoshopBrushes(all)) {
    std::vector<Tool> brushes = decodePhotoshopBrushes(all);
    if (brushes.empty()) return std::nullopt;
    return std::move(brushes.front());
  }

  // A bare description, with no images beside it: a brush whose whole
  // definition is its numbers.
  if (all.front() == std::byte{'{'})
    return assembleBrush(all, {}, {});

  (void)hint;
  return std::nullopt;
}

std::string encodeBrush(const Tool& tool) {
  std::string out = "{\n";
  out += "  \"tip\": \"";
  out += wordFor(tool.tip, kTipNames);
  out += "\",\n";
  out += "  \"color\": [" + decimal(tool.color.fR) + ", " +
         decimal(tool.color.fG) + ", " + decimal(tool.color.fB) + ", " +
         decimal(tool.color.fA) + "],\n";
  out += "  \"width\": " + decimal(tool.width) + ",\n";
  out += "  \"opacity\": " + decimal(tool.opacity) + ",\n";
  out += "  \"spacing\": " + decimal(tool.spacing) + ",\n";
  out += "  \"scatter\": " + decimal(tool.scatter) + ",\n";
  out += "  \"density\": " + decimal(tool.density) + ",\n";
  out += "  \"angle\": " + decimal(tool.angle) + ",\n";
  out += "  \"aspect\": " + decimal(tool.aspect) + ",\n";
  out += "  \"sizeJitter\": " + decimal(tool.sizeJitter) + ",\n";
  out += "  \"opacityJitter\": " + decimal(tool.opacityJitter) + ",\n";
  out += "  \"spacingJitter\": " + decimal(tool.spacingJitter) + ",\n";
  out += "  \"rotation\": \"";
  out += wordFor(tool.rotation, kRotationNames);
  out += "\"";

  if (tool.shape) {
    out += ",\n  \"shape\": {\"image\": \"";
    out += kShapeName;
    out += "\", \"mask\": \"";
    out += wordFor(tool.shape->mask, kMaskNames);
    out += "\", \"spacing\": " + decimal(tool.shape->spacing);
    out += ", \"scatter\": " + decimal(tool.shape->scatter);
    out += ", \"angleJitter\": " + decimal(tool.shape->angleJitter) + "}";
  }
  if (tool.grain) {
    out += ",\n  \"grain\": {\"image\": \"";
    out += kGrainName;
    out += "\", \"space\": \"";
    out += wordFor(tool.grain->space, kSpaceNames);
    out += "\", \"scale\": " + decimal(tool.grain->scale);
    out += ", \"depth\": " + decimal(tool.grain->depth) + "}";
  }
  if (!tool.dynamics.empty()) {
    out += ",\n  \"dynamics\": {\n";
    bool first = true;
    const auto put = [&](std::string_view name,
                         const std::optional<Response>& response) {
      if (!response) return;
      if (!first) out += ",\n";
      writeResponse(out, name, *response);
      first = false;
    };
    put("size", tool.dynamics.size);
    put("opacity", tool.dynamics.opacity);
    put("flow", tool.dynamics.flow);
    out += "\n  }";
  }
  out += "\n}\n";
  return out;
}

}  // namespace sigil::draw::brush::format
