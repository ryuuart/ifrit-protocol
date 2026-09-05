/** @file
 * The stock catalogue and the operations on any catalogue.
 */

#include <sigildraw/brush/Catalogue.h>

#include <algorithm>
#include <utility>

namespace sigil::draw::brush {

namespace {

Tool dryTool(float width, float scatter, float sharpness, float grain,
             float opacity, float spacing, Pressure pressure) {
  Tool tool = pencil(SkColors::kBlack, width);
  tool.scatter = scatter;
  tool.sharpness = sharpness;
  tool.grain = grain;
  tool.opacity = opacity;
  tool.spacing = spacing;
  tool.pressure = std::move(pressure);
  return tool;
}

Pressure bell(float centerJitter, float widthJitter, float minimum,
              float maximum) {
  return Pressure::gaussianProfile(centerJitter, widthJitter, minimum, maximum);
}

}  // namespace

Catalogue Catalogue::stock() {
  Catalogue catalogue;
  catalogue.add("pen", dryTool(0.30f, 0.15f, 0.90f, 0.70f, 150.0f / 255.0f,
                               0.10f, bell(0.15f, 0.20f, 1.00f, 1.20f)));
  catalogue.add("rotring", dryTool(0.15f, 0.05f, 0.70f, 0.90f, 210.0f / 255.0f,
                                   0.10f, bell(0.35f, 0.20f, 1.00f, 1.30f)));
  catalogue.add("2B", dryTool(0.30f, 0.75f, 0.45f, 0.80f, 180.0f / 255.0f,
                              0.10f, bell(0.10f, 0.30f, 0.90f, 1.10f)));
  catalogue.add("HB", dryTool(0.30f, 0.60f, 0.30f, 0.70f, 170.0f / 255.0f,
                              0.10f, bell(0.15f, 0.20f, 0.90f, 1.10f)));
  catalogue.add("2H", dryTool(0.20f, 0.60f, 0.30f, 0.75f, 120.0f / 255.0f,
                              0.10f, bell(0.15f, 0.20f, 0.90f, 1.10f)));
  catalogue.add("cpencil", dryTool(0.35f, 0.55f, 0.80f, 0.70f, 75.0f / 255.0f,
                                   0.10f, bell(0.15f, 0.20f, 0.95f, 1.10f)));

  catalogue.add("pastel",
                dryTool(0.70f, 5.00f, 0.91f, 1.00f, 30.0f / 255.0f,
                        0.085f / 3.0f, bell(0.40f, 0.05f, 0.93f, 1.09f)));
  catalogue.add("crayon", dryTool(0.33f, 1.90f, 0.75f, 2.00f, 159.0f / 255.0f,
                                  0.07f, Pressure{1.10f, 1.00f, 0.90f}));
  catalogue.add("charcoal",
                dryTool(0.35f, 1.50f, 0.68f, 2.00f, 120.0f / 255.0f, 0.03f,
                        bell(0.15f, 0.40f, 0.95f, 1.10f)));

  Tool aerosol = spray(SkColors::kBlack, 50.0f);
  aerosol.width = 0.20f;
  aerosol.scatter = 6.0f;
  aerosol.opacity = 90.0f / 255.0f;
  aerosol.spacing = 0.50f;
  aerosol.grain = 1.0f;
  aerosol.bristles = 40;
  aerosol.pressure = bell(0.20f, 0.35f, 0.70f, 1.00f);
  catalogue.add("spray", aerosol);

  Tool flat = marker(SkColors::kBlack, 2.0f);
  flat.spacing = 0.03f;
  flat.scatter = 0.20f;
  flat.opacity = 1.0f / 255.0f;
  flat.pressure = bell(0.35f, 0.25f, 0.85f, 1.20f);
  catalogue.add("marker", flat);
  Tool marker2 = flat;
  marker2.width = 2.8f;
  marker2.scatter = 0.28f;
  marker2.opacity = 0.30f;
  catalogue.add("marker2", marker2);

  Tool hatch = dryTool(0.20f, 0.08f, 0.92f, 0.88f, 0.72f, 0.10f,
                       Pressure{1.0f, 1.0f, 1.0f});
  catalogue.add("hatch_brush", hatch);
  return catalogue;
}

const Tool* Catalogue::add(std::string name, Tool tool) {
  if (name.empty()) return nullptr;
  return &m_tools.insert_or_assign(std::move(name), std::move(tool))
              .first->second;
}

const Tool* Catalogue::find(std::string_view name) const {
  const auto found = m_tools.find(name);
  return found == m_tools.end() ? nullptr : &found->second;
}

bool Catalogue::contains(std::string_view name) const {
  return find(name) != nullptr;
}

std::vector<std::string> Catalogue::names() const {
  std::vector<std::string> result;
  result.reserve(m_tools.size());
  for (const auto& [name, unused] : m_tools) result.push_back(name);
  std::ranges::sort(result);
  return result;
}

void Catalogue::scale(float factor) {
  factor = std::max(0.0f, factor);
  for (auto& [unused, tool] : m_tools) {
    tool.width *= factor;
    tool.scatter *= factor;
    tool.spacing *= factor;
  }
}

}  // namespace sigil::draw::brush
