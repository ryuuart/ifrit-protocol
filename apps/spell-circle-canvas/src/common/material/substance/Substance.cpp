/** @file
 * A cooked graph as a Material: the cook held by a base part that takes
 * the graph's inputs through the Material's own `set()` and `bind()`, the
 * base-colour output layered over it (masked by the opacity output), and
 * the other outputs filling the surface channels by the usage each
 * declares. The description, a lone output and the wait for pending
 * cooks read the same part. Built the same with or without the SDK; the
 * cook it stands on answers empty without one.
 */

#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilmaterial/substance/advanced/Archive.h>
#include <sigilmaterial/substance/advanced/Cook.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilio/hub/Hub.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sigil::material {

namespace sbsar {

namespace {

/** THE COOK AS A MATERIAL'S BASE. It draws nothing itself — the outputs
 *  layered over it and in its surface are what show — and it is the part
 *  `Material::set()` and `bind()` reach, so a graph input is written
 *  through the same entrance as a program's field. Two parts are equal
 *  when they are one cook with the same values written and bound. */
class GraphPart final : public detail::Part {
 public:
  explicit GraphPart(CookScheduler cook) : cook(std::move(cook)) {}

  bool equals(const detail::Part& other) const override {
    const auto* same = dynamic_cast<const GraphPart*>(&other);
    return same && same->cook == cook && same->written == written &&
           same->bound == bound;
  }
  bool isRunning() const override { return cook.isRunning(); }
  bool geometryDependent() const override { return false; }

  std::shared_ptr<const detail::Part> withInput(
      std::string_view name, std::span<const float> values) const override {
    CookScheduler writer = cook;
    if (!writer.set(name, values)) return nullptr;
    writer.cook();
    auto next = std::make_shared<GraphPart>(*this);
    next->record(name, std::vector<float>(values.begin(), values.end()));
    return next;
  }

  std::shared_ptr<const detail::Part> withBinding(
      std::string_view name, motion::Animatable<float> value) const override {
    CookScheduler writer = cook;
    // Only a one-number input can follow one number.
    if (writer.get(name).size() != 1) return nullptr;
    writer.bind(name, value);
    writer.follow();
    auto next = std::make_shared<GraphPart>(*this);
    std::erase_if(next->bound,
                  [&](const auto& entry) { return entry.first == name; });
    next->bound.emplace_back(std::string(name), std::move(value));
    return next;
  }

  void record(std::string_view name, std::vector<float> values) {
    for (auto& [identifier, stated] : written)
      if (identifier == name) {
        stated = std::move(values);
        return;
      }
    written.emplace_back(std::string(name), std::move(values));
  }

  CookScheduler cook;
  std::vector<std::pair<std::string, std::vector<float>>> written;
  std::vector<std::pair<std::string, motion::Animatable<float>>> bound;
};

/** The empty material an entrance answers when there is nothing to cook. */
Material nothing() { return Color{0, 0, 0, 0}; }

const GraphPart* graphOf(const Material& material) {
  return dynamic_cast<const GraphPart*>(material.source());
}

/** The usages the material reads, in the order it reads them. */
constexpr std::array<std::string_view, 8> kMaterialUsages = {
    "baseColor", "diffuse",  "opacity",          "normal",
    "roughness", "metallic", "ambientOcclusion", "emissive"};

/** The `.sbsprs` file of the same name beside @p uri. */
std::string presetFileBeside(std::string_view uri) {
  std::string sibling(uri);
  const size_t dot = sibling.rfind('.');
  const size_t slash = sibling.find_last_of("/\\");
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
    sibling.erase(dot);
  return sibling + ".sbsprs";
}

void addPresetsBeside(io::Hub& hub, std::string_view uri,
                      CookScheduler& cook) {
  if (std::optional<std::string> text = hub.text(presetFileBeside(uri)))
    cook.addPresets(*text);
}

}  // namespace

Description describe(io::Hub& hub, std::string_view uri,
                     std::string_view graph) {
  if (!available()) return {};
  const std::shared_ptr<const Archive> archive = load(hub, uri);
  if (!archive) return {};
  const std::optional<size_t> index = archive->find(graph);
  if (!index) return {};
  Description description = archive->graph(*index);
  CookScheduler cook(archive, *index);
  addPresetsBeside(hub, uri, cook);
  description.presets = cook.presets();
  return description;
}

media::PixelSource output(const Material& material, std::string_view usage) {
  const GraphPart* part = graphOf(material);
  return part ? part->cook.output(usage) : media::PixelSource{};
}

void settle(const Material& material) {
  const GraphPart* part = graphOf(material);
  if (!part) return;
  part->cook.follow();
  part->cook.wait();
}

}  // namespace sbsar

Material substance(io::Hub& hub, std::string_view uri,
                   SubstanceOptions options) {
  const std::string where(uri);
  if (!sbsar::available()) {
    reportOnce("substance:unavailable",
               "this build has no Substance SDK; \"" + where +
                   "\" answers an empty material");
    return sbsar::nothing();
  }
  const std::shared_ptr<const sbsar::Archive> archive = sbsar::load(hub, uri);
  if (!archive) {
    reportOnce("substance:read:" + where,
               "\"" + where + "\" is not a Substance archive that can be read");
    return sbsar::nothing();
  }
  const std::optional<size_t> graph = archive->find(options.graph);
  if (!graph) {
    reportOnce("substance:graph:" + where + ":" + options.graph,
               "\"" + where + "\" holds no graph \"" + options.graph + "\"");
    return sbsar::nothing();
  }

  sbsar::CookOptions cooking{.resolution = options.resolution,
                             .seed = options.seed,
                             .outputs = options.outputs,
                             .engine = options.engine};
  if (cooking.outputs.empty()) {
    for (const sbsar::Output& output : archive->graph(*graph).outputs)
      if (output.image && std::find(sbsar::kMaterialUsages.begin(),
                                    sbsar::kMaterialUsages.end(),
                                    output.usage) != sbsar::kMaterialUsages.end())
        cooking.outputs.push_back({output.usage});
  }
  sbsar::CookScheduler cook(archive, *graph, cooking);

  if (!options.preset.empty()) {
    sbsar::addPresetsBeside(hub, uri, cook);
    if (!cook.applyPreset(options.preset))
      reportOnce("substance:preset:" + where + ":" + options.preset,
                 "\"" + where + "\" has no preset \"" + options.preset +
                     "\"; the authored values stand");
  }
  auto part = std::make_shared<sbsar::GraphPart>(cook);
  for (const sbsar::InputValue& input : options.inputs) {
    if (!cook.set(input.identifier, input.values)) {
      reportOnce("substance:input:" + where + ":" + input.identifier,
                 "\"" + where + "\" has no input \"" + input.identifier +
                     "\" taking " + std::to_string(input.values.size()) +
                     " numbers; the value is ignored");
      continue;
    }
    part->record(input.identifier, input.values);
  }
  cook.cookNow();

  Material material(std::shared_ptr<const detail::Part>(std::move(part)));
  const auto channel = [&](std::string_view usage) -> std::optional<Material> {
    media::PixelSource pixels = cook.output(usage);
    if (!pixels) return std::nullopt;
    return image(std::move(pixels));
  };
  std::optional<Material> base = channel("baseColor");
  if (!base) base = channel("diffuse");
  if (base) {
    LayerOptions how;
    if (std::optional<Material> opacity = channel("opacity"))
      how.mask = Mask{.source = std::move(*opacity),
                      .channel = MaskChannel::Luminance};
    material.layer(std::move(*base), how);
  }
  SurfaceOptions surface;
  bool lit = false;
  if (std::optional<Material> normal = channel("normal")) {
    surface.normal = std::move(normal);
    surface.normalDirectX = cook.normalsAreDirectX();
    lit = true;
  }
  const auto fill = [&](std::string_view usage, Channel& into) {
    if (std::optional<Material> map = channel(usage)) {
      into = std::move(*map);
      lit = true;
    }
  };
  fill("roughness", surface.roughness);
  fill("metallic", surface.metallic);
  fill("ambientOcclusion", surface.occlusion);
  if (std::optional<Material> emissive = channel("emissive")) {
    surface.emissionMap = std::move(emissive);
    surface.emission = Color{1, 1, 1, 1};
    surface.emissionStrength = 1;
    lit = true;
  }
  if (lit) material.surface(surface);
  return material;
}

}  // namespace sigil::material
