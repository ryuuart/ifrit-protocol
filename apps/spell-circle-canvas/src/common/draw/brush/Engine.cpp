/** @file
 * The engine's state: catalogue, selection, interiors, field, clip, and
 * the shaping every stroke goes through.
 */

#include "PenUnits.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Engine.h>
#include <sigildraw/brush/Fields.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sigil::draw::brush {

namespace {

/** The spacing a polygon's boundary is resampled at before a field bends
 *  it: fine enough that a bend reads as a curve. */
constexpr float kBoundarySpacing = 5.0f;

}  // namespace

Engine::Engine() : m_catalogue(Catalogue::stock()) {
  m_massTool = *m_catalogue.find("crayon");
  for (auto& [name, field] : stockFields())
    m_fields.emplace(std::move(name), std::move(field));
}

Engine::Engine(Catalogue catalogue) : m_catalogue(std::move(catalogue)) {
  const std::vector<std::string> names = m_catalogue.names();
  if (!m_catalogue.contains(m_selected) && !names.empty())
    m_selected = names.front();
  m_massTool = definition() ? *definition() : Tool{};
  for (auto& [name, field] : stockFields())
    m_fields.emplace(std::move(name), std::move(field));
}

// ---- the catalogue and the selection ----------------------------------------

const Tool* Engine::add(std::string name, Tool tool) {
  return m_catalogue.add(std::move(name), std::move(tool));
}

std::vector<std::string> Engine::names() const { return m_catalogue.names(); }

void Engine::scaleBrushes(float factor) { m_catalogue.scale(factor); }

const Tool* Engine::pick(std::string_view name) {
  const Tool* found = m_catalogue.find(name);
  if (found) m_selected = name;
  return found;
}

const Tool* Engine::set(std::string_view name, SkColor4f color, float weight) {
  const Tool* found = pick(name);
  if (!found) return nullptr;
  m_color = color;
  m_weight = std::max(0.0f, weight);
  m_strokeActive = true;
  return found;
}

void Engine::stroke(SkColor4f color) {
  m_color = color;
  m_strokeActive = true;
}

void Engine::noStroke() { m_strokeActive = false; }

void Engine::strokeWeight(float weight) { m_weight = std::max(0.0f, weight); }

bool Engine::hasStroke() const {
  return m_strokeActive && m_weight > 0.0f && definition();
}

const Tool* Engine::definition() const { return m_catalogue.find(m_selected); }

Tool Engine::tool() const { return tool(false); }

Tool Engine::tool(bool devicePressure) const {
  const Tool* found = definition();
  if (!found) return {};
  Tool result = *found;
  result.color = m_color;
  result.width *= m_weight;
  result.scatter *= m_weight;
  if (devicePressure) {
    result.pressure = {1, 1, 1};
    result.pressure.variation.reset();
  }
  return result;
}

Tool Engine::hatchTool() const { return m_hatchTool ? *m_hatchTool : tool(); }

// ---- the interiors ----------------------------------------------------------

void Engine::fill(SkColor4f color, float opacity) {
  m_fill.color = color;
  m_fill.opacity = std::clamp(opacity, 0.0f, 1.0f);
  m_fillActive = true;
}

void Engine::noFill() { m_fillActive = false; }

void Engine::wash(SkColor4f color, float opacity) {
  m_washColor = color;
  m_washOpacity = std::clamp(opacity, 0.0f, 1.0f);
  m_washActive = true;
}

void Engine::noWash() { m_washActive = false; }

void Engine::fillBleed(float bleed, BleedDirection direction,
                       std::optional<float> angle) {
  m_fill.bleed = std::clamp(bleed, 0.0f, 1.0f);
  m_fill.bleedDirection = direction;
  m_fill.bleedAngle = angle;
}

void Engine::fillTexture(float texture, float border, bool scatter) {
  m_fill.texture = std::clamp(texture, 0.0f, 1.0f);
  m_fill.border = std::clamp(border, 0.0f, 1.0f);
  m_fill.scatter = scatter;
}

void Engine::hatch(const Hatch& style) {
  if (!(style.spacing > 0.0f)) return;
  m_hatch = style;
}

void Engine::hatch(const Pen& pen, float spacing, float angle, float jitter,
                   float gradient, bool continuous) {
  hatch(Hatch{.spacing = spacing,
              .angle = toRadians(pen, angle),
              .jitter = jitter,
              .gradient = gradient,
              .continuous = continuous});
}

void Engine::noHatch() {
  m_hatch.reset();
  m_hatchTool.reset();
}

const Tool* Engine::hatchStyle(std::string_view name, SkColor4f color,
                               float weight) {
  const Tool* found = m_catalogue.find(name);
  if (!found) return nullptr;
  Tool tool = *found;
  tool.color = color;
  tool.width *= std::max(0.0f, weight);
  tool.scatter *= std::max(0.0f, weight);
  m_hatchTool = std::move(tool);
  return found;
}

const Tool* Engine::mass(std::string_view name, SkColor4f color,
                         const Mass& style) {
  const Tool* found = m_catalogue.find(name);
  if (!found) return nullptr;
  m_massTool = *found;
  m_massTool.color = color;
  m_mass = style;
  return found;
}

void Engine::noMass() { m_mass.reset(); }

// ---- the field --------------------------------------------------------------

bool Engine::addField(std::string name, Direction field, Constant units) {
  if (name.empty() || !field) return false;
  if (units != RADIANS && units != DEGREES) return false;
  if (units == DEGREES) {
    field = [source = std::move(field)](SkPoint point, float seconds) {
      return radians(source(point, seconds));
    };
  }
  m_fields.insert_or_assign(std::move(name), std::move(field));
  return true;
}

std::vector<std::string> Engine::listFields() const {
  std::vector<std::string> result;
  result.reserve(m_fields.size());
  for (const auto& [name, unused] : m_fields) result.push_back(name);
  std::ranges::sort(result);
  return result;
}

bool Engine::field(std::string_view name) {
  if (!m_fields.contains(name)) return false;
  m_selectedField = name;
  return true;
}

void Engine::noField() { m_selectedField.clear(); }

void Engine::wiggle(float amount) {
  m_fieldAmount = amount;
  m_selectedField = "hand";
}

const Direction* Engine::activeField() const {
  if (m_selectedField.empty()) return nullptr;
  const auto found = m_fields.find(m_selectedField);
  return found == m_fields.end() ? nullptr : &found->second;
}

// ---- the clip and the state -------------------------------------------------

void Engine::clip(SkRect region) {
  m_clip = Clip{.region = region.makeSorted()};
}

void Engine::clip(const Pen& pen, SkRect region) {
  m_clip = Clip{.region = region.makeSorted()};
  if (const SkCanvas* canvas = pen.canvas())
    m_clip->transform = canvas->getTotalMatrix();
}

void Engine::noClip() { m_clip.reset(); }

void Engine::applyClip(Pen& pen) const {
  if (!m_clip) return;
  if (m_clip->transform) {
    SkCanvas* canvas = pen.canvas();
    if (!canvas) return;
    const SkPath devicePath =
        SkPath::Rect(m_clip->region).makeTransform(*m_clip->transform);
    const SkMatrix current = canvas->getTotalMatrix();
    canvas->resetMatrix();
    canvas->clipPath(devicePath, SkClipOp::kIntersect, true);
    canvas->setMatrix(current);
    return;
  }
  const SkRect region = m_clip->region;
  pen.clip([&] { pen.shape(SkPath::Rect(region)); });
}

void Engine::push() {
  m_states.push_back({.selected = m_selected,
                      .color = m_color,
                      .weight = m_weight,
                      .strokeActive = m_strokeActive,
                      .fill = m_fill,
                      .fillActive = m_fillActive,
                      .washColor = m_washColor,
                      .washOpacity = m_washOpacity,
                      .washActive = m_washActive,
                      .hatch = m_hatch,
                      .hatchTool = m_hatchTool,
                      .mass = m_mass,
                      .massTool = m_massTool,
                      .selectedField = m_selectedField,
                      .fieldAmount = m_fieldAmount,
                      .clip = m_clip});
}

void Engine::pop() {
  if (m_states.empty()) return;
  State state = std::move(m_states.back());
  m_states.pop_back();
  m_selected = std::move(state.selected);
  m_color = state.color;
  m_weight = state.weight;
  m_strokeActive = state.strokeActive;
  m_fill = std::move(state.fill);
  m_fillActive = state.fillActive;
  m_washColor = state.washColor;
  m_washOpacity = state.washOpacity;
  m_washActive = state.washActive;
  m_hatch = state.hatch;
  m_hatchTool = std::move(state.hatchTool);
  m_mass = state.mass;
  m_massTool = std::move(state.massTool);
  m_selectedField = std::move(state.selectedField);
  m_fieldAmount = state.fieldAmount;
  m_clip = state.clip;
}

// ---- shaping ----------------------------------------------------------------

Stroke Engine::shaped(const Pen& pen, std::span<const Sample> path,
                      bool applyField) const {
  Stroke result(path.begin(), path.end());
  const Direction* field = applyField ? activeField() : nullptr;
  if (!field || path.size() < 2) return result;
  const float seconds = fieldSeconds(pen);
  // Each segment keeps its length and turns by the field's answer where
  // the previous shaped sample landed.
  for (size_t index = 1; index < path.size(); ++index) {
    const SkPoint sourceFrom = path[index - 1].position;
    const SkPoint sourceTo = path[index].position;
    const float dx = sourceTo.fX - sourceFrom.fX;
    const float dy = sourceTo.fY - sourceFrom.fY;
    const float length = std::hypot(dx, dy);
    const float base = length > 0.0f ? std::atan2(dy, dx) : 0.0f;
    const SkPoint from = result[index - 1].position;
    const float influence = (*field)(from, seconds) * m_fieldAmount;
    result[index].position = {from.fX + std::cos(base + influence) * length,
                              from.fY + std::sin(base + influence) * length};
  }
  // A closed path stays closed: the drift the bends accumulated is taken
  // back out in proportion along the path.
  if (path.front().position == path.back().position && result.size() > 2) {
    const float driftX = result.back().position.fX - result.front().position.fX;
    const float driftY = result.back().position.fY - result.front().position.fY;
    const float denominator = (float)(result.size() - 1);
    for (size_t index = 1; index < result.size(); ++index) {
      const float progress = (float)index / denominator;
      result[index].position.fX -= driftX * progress;
      result[index].position.fY -= driftY * progress;
    }
  }
  return result;
}

Stroke Engine::shapedBoundary(const Pen& pen, std::span<const Sample> corners,
                              bool applyField) const {
  if (corners.empty()) return {};
  const bool closed = corners.front().position == corners.back().position;
  if (!applyField || !activeField()) {
    Stroke result(corners.begin(), corners.end());
    if (!closed) result.push_back(result.front());
    return result;
  }
  Stroke boundary;
  const size_t edges = closed ? corners.size() - 1 : corners.size();
  for (size_t edge = 0; edge < edges; ++edge) {
    const Sample& from = corners[edge];
    const Sample& to = corners[(edge + 1) % corners.size()];
    const Stroke side = segment(from.position, to.position, kBoundarySpacing,
                                from.pressure, to.pressure);
    boundary.insert(boundary.end(), side.begin(), side.end() - 1);
  }
  boundary.push_back(boundary.front());
  return shaped(pen, boundary, true);
}

void Engine::paintStroke(Pen& pen, const Tool& tool,
                         std::span<const Sample> path, bool applyField) const {
  if (!hasStroke()) return;
  const Stroke result = shaped(pen, path, applyField);
  pen.push();
  applyClip(pen);
  brush::paint(pen, tool, result);
  pen.pop();
}

}  // namespace sigil::draw::brush
