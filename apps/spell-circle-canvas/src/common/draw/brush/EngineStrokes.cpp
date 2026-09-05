/** @file
 * The engine's strokes: stored, relative and live.
 */

#include "PenUnits.h"

#include <sigildraw/Pen.h>
#include <sigildraw/brush/Engine.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

// ---- stored strokes ---------------------------------------------------------

void Engine::paint(Pen& pen, std::span<const Sample> path) const {
  paintStroke(pen, tool(), path, true);
}

void Engine::line(Pen& pen, SkPoint from, SkPoint to, float startPressure,
                  float endPressure) const {
  if (!hasStroke() || from == to) return;
  const Tool current = tool();
  paintStroke(pen, current,
              segment(from, to, spacingOf(current), startPressure, endPressure),
              true);
}

void Engine::flowLine(Pen& pen, SkPoint start, float length,
                      float direction) const {
  if (!hasStroke() || !(length > 0.0f)) return;
  direction = toRadians(pen, direction);
  const Tool current = tool();
  paintStroke(pen, current,
              segment(start,
                      {start.fX + std::cos(direction) * length,
                       start.fY + std::sin(direction) * length},
                      spacingOf(current)),
              true);
}

PlacedPlot Engine::spline(Pen& pen, std::span<const Sample> controls,
                          float curvature) const {
  const Tool current = tool();
  const Stroke curve = brush::spline(controls, spacingOf(current), curvature);
  if (curve.empty()) return PlacedPlot{};
  paintStroke(pen, current, curve, true);
  return {Plot::fromStroke(curve, PlotType::Segments), curve.front().position};
}

// ---- live input -------------------------------------------------------------

void Engine::depositInput(Pen& pen, std::span<const Dab> sampled, bool last) {
  if (!m_liveTool || sampled.empty()) return;
  std::vector<Dab> marks(sampled.begin(), sampled.end());
  if (const Direction* field = activeField()) {
    // Each dab keeps its distance from the last and turns by the field's
    // answer where the last shaped dab landed, as a stored stroke does.
    const float seconds = fieldSeconds(pen);
    std::optional<Dab> source = m_liveLastSourceDab;
    std::optional<Dab> shapedDab = m_liveLastDab;
    for (Dab& dab : marks) {
      const Dab unshaped = dab;
      if (source && shapedDab) {
        const float dx = unshaped.position.fX - source->position.fX;
        const float dy = unshaped.position.fY - source->position.fY;
        const float length = std::hypot(dx, dy);
        const float base = length > 0.0f ? std::atan2(dy, dx) : dab.direction;
        const float influence =
            (*field)(shapedDab->position, seconds) * m_fieldAmount;
        dab.direction = base + influence;
        dab.position = {shapedDab->position.fX + std::cos(dab.direction) * length,
                        shapedDab->position.fY + std::sin(dab.direction) * length};
      }
      source = unshaped;
      shapedDab = dab;
    }
    m_liveLastSourceDab = source;
  }
  // A fibre tip joins its hairs across runs, so the run starts where the
  // last one ended.
  if (m_liveTool->tip == Tip::Fibres && m_liveLastDab && m_liveDeposited)
    marks.insert(marks.begin(), *m_liveLastDab);

  pen.push();
  applyClip(pen);
  deposit(pen, *m_liveTool, marks, {.start = !m_liveDeposited, .end = last});
  pen.pop();
  m_liveDeposited = true;
  m_liveLastDab = marks.back();
}

void Engine::beginInput(Pen& pen, Input input) {
  if (!hasStroke()) return;
  m_liveTool = prepareStroke(pen, tool(true));
  m_liveDeposited = false;
  m_liveLastDab.reset();
  m_liveLastSourceDab.reset();
  depositInput(pen, m_sampler.begin(input), false);
}

void Engine::moveInput(Pen& pen, Input input) {
  if (!hasStroke() || !m_sampler.active() || !m_liveTool) return;
  depositInput(pen, m_sampler.move(input, spacingOf(*m_liveTool)), false);
}

void Engine::endInput(Pen& pen, Input input) {
  if (!hasStroke() || !m_sampler.active() || !m_liveTool) return;
  depositInput(pen, m_sampler.end(input, spacingOf(*m_liveTool)), true);
  cancelInput();
}

void Engine::cancelInput() {
  m_sampler.cancel();
  m_liveTool.reset();
  m_liveDeposited = false;
  m_liveLastDab.reset();
  m_liveLastSourceDab.reset();
}

// ---- relative strokes -------------------------------------------------------

void Engine::beginStroke(PlotType kind, SkPoint position) {
  m_strokeKind = kind;
  m_stroke.clear();
  m_stroke.push_back({position, 1.0f});
}

void Engine::move(const Pen& pen, float angle, float length, float pressure) {
  if (!m_strokeKind || m_stroke.empty() || !(length >= 0.0f)) return;
  angle = toRadians(pen, angle);
  const SkPoint from = m_stroke.back().position;
  m_stroke.push_back(
      {{from.fX + std::cos(angle) * length, from.fY + std::sin(angle) * length},
       std::max(0.0f, pressure)});
}

Stroke Engine::endStroke(Pen& pen, float angle, float pressure) {
  if (!m_strokeKind || m_stroke.empty()) return {};
  angle = toRadians(pen, angle);
  m_stroke.back().pressure = std::max(0.0f, pressure);
  const float spacing = definition() ? spacingOf(*definition()) : 1.0f;
  Stroke result = *m_strokeKind == PlotType::Curve
                      ? brush::spline(m_stroke, spacing, 0.65f)
                      : m_stroke;
  if (result.size() >= 2 && *m_strokeKind == PlotType::Curve) {
    // The end angle turns the last step of the curve.
    const SkPoint before = result[result.size() - 2].position;
    const SkPoint end = result.back().position;
    const float tail = std::min(
        std::hypot(end.fX - before.fX, end.fY - before.fY), spacing);
    result.back().position = {before.fX + std::cos(angle) * tail,
                              before.fY + std::sin(angle) * tail};
  }
  paint(pen, result);
  cancelStroke();
  return result;
}

void Engine::cancelStroke() {
  m_strokeKind.reset();
  m_stroke.clear();
}

}  // namespace sigil::draw::brush
