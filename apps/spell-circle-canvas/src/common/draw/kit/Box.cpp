/** @file
 * Stock and user-defined brush catalogue.
 */

#include <sigildraw/kit/Box.h>

#include <algorithm>
#include <utility>

namespace sigil::draw::brush {

namespace {

Brush dryTool(float width, float scatter, float sharpness, float grain,
              float opacity, float spacing, Pressure pressure) {
  Brush tool = pencil(SkColors::kBlack, width);
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

Box Box::stock() {
  Box box;
  box.add("pen", dryTool(0.30f, 0.15f, 0.90f, 0.70f, 150.0f / 255.0f, 0.10f,
                         bell(0.15f, 0.20f, 1.00f, 1.20f)));
  box.add("rotring", dryTool(0.15f, 0.05f, 0.70f, 0.90f, 210.0f / 255.0f, 0.10f,
                             bell(0.35f, 0.20f, 1.00f, 1.30f)));
  box.add("2B", dryTool(0.30f, 0.75f, 0.45f, 0.80f, 180.0f / 255.0f, 0.10f,
                        bell(0.10f, 0.30f, 0.90f, 1.10f)));
  box.add("HB", dryTool(0.30f, 0.60f, 0.30f, 0.70f, 170.0f / 255.0f, 0.10f,
                        bell(0.15f, 0.20f, 0.90f, 1.10f)));
  box.add("2H", dryTool(0.20f, 0.60f, 0.30f, 0.75f, 120.0f / 255.0f, 0.10f,
                        bell(0.15f, 0.20f, 0.90f, 1.10f)));
  box.add("cpencil", dryTool(0.35f, 0.55f, 0.80f, 0.70f, 75.0f / 255.0f, 0.10f,
                             bell(0.15f, 0.20f, 0.95f, 1.10f)));

  box.add("pastel", dryTool(0.70f, 5.00f, 0.91f, 1.00f, 30.0f / 255.0f,
                            0.085f / 3.0f, bell(0.40f, 0.05f, 0.93f, 1.09f)));
  box.add("crayon", dryTool(0.33f, 1.90f, 0.75f, 2.00f, 159.0f / 255.0f, 0.07f,
                            Pressure{1.10f, 1.00f, 0.90f}));
  box.add("charcoal", dryTool(0.35f, 1.50f, 0.68f, 2.00f, 120.0f / 255.0f,
                              0.03f, bell(0.15f, 0.40f, 0.95f, 1.10f)));

  Brush aerosol = spray(SkColors::kBlack, 50.0f);
  aerosol.width = 0.20f;
  aerosol.scatter = 6.0f;
  aerosol.opacity = 90.0f / 255.0f;
  aerosol.spacing = 0.50f;
  aerosol.grain = 1.0f;
  aerosol.bristles = 40;
  aerosol.pressure = bell(0.20f, 0.35f, 0.70f, 1.00f);
  box.add("spray", aerosol);

  Brush flat = marker(SkColors::kBlack, 2.0f);
  flat.spacing = 0.03f;
  flat.scatter = 0.20f;
  flat.opacity = 1.0f / 255.0f;
  flat.pressure = bell(0.35f, 0.25f, 0.85f, 1.20f);
  box.add("marker", flat);
  Brush marker2 = flat;
  marker2.width = 2.8f;
  marker2.scatter = 0.28f;
  marker2.opacity = 0.30f;
  box.add("marker2", marker2);

  Brush hatch = dryTool(0.20f, 0.08f, 0.92f, 0.88f, 0.72f, 0.10f,
                        Pressure{1.0f, 1.0f, 1.0f});
  hatch.grain = 0.88f;
  box.add("hatch_brush", hatch);
  return box;
}

bool Box::add(std::string name, Brush brush) {
  if (name.empty()) return false;
  m_brushes.insert_or_assign(std::move(name), std::move(brush));
  return true;
}

const Brush* Box::find(std::string_view name) const {
  const auto found = m_brushes.find(std::string(name));
  return found == m_brushes.end() ? nullptr : &found->second;
}

bool Box::contains(std::string_view name) const {
  return find(name) != nullptr;
}

std::vector<std::string> Box::names() const {
  std::vector<std::string> result;
  result.reserve(m_brushes.size());
  for (const auto& [name, unused] : m_brushes) result.push_back(name);
  std::ranges::sort(result);
  return result;
}

void Box::scale(float factor) {
  factor = std::max(0.0f, factor);
  for (auto& [unused, brush] : m_brushes) {
    brush.width *= factor;
    brush.scatter *= factor;
    brush.spacing *= factor;
  }
}

}  // namespace sigil::draw::brush
