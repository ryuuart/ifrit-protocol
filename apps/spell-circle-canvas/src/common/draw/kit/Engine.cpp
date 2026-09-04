/** @file
 * Instance-owned brush selection and in-progress stroke state.
 */

#include <include/core/SkPathBuilder.h>
#include <sigildraw/Math.h>
#include <sigildraw/kit/Engine.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace sigil::draw::brush {

namespace {

boost::unordered_flat_map<std::string, Engine::Field> stockFields() {
  using std::numbers::pi_v;
  boost::unordered_flat_map<std::string, Engine::Field> result;
  result.emplace("hand", [](SkPoint point, float seconds) {
    return std::sin(point.fX * 0.045f + point.fY * 0.021f + seconds) * 0.18f;
  });
  result.emplace("curved", [](SkPoint point, float) {
    return std::atan2(point.fY, point.fX) + pi_v<float> * 0.5f;
  });
  result.emplace("zigzag", [](SkPoint point, float) {
    return std::sin(point.fX * 0.08f) >= 0.0f ? 0.55f : -0.55f;
  });
  result.emplace("waves", [](SkPoint point, float seconds) {
    return std::sin(point.fX * 0.018f + seconds) * 0.42f;
  });
  result.emplace("seabed", [](SkPoint point, float seconds) {
    return -0.18f +
           std::sin(point.fX * 0.011f + point.fY * 0.006f + seconds) * 0.24f;
  });
  result.emplace("spiral", [](SkPoint point, float) {
    return std::atan2(point.fY, point.fX) + pi_v<float> * 0.42f;
  });
  result.emplace("columns", [](SkPoint point, float) {
    return pi_v<float> * 0.5f + std::sin(point.fY * 0.025f) * 0.12f;
  });
  return result;
}

std::vector<SkPoint> circlePoints(float x, float y, float radius, float start,
                                  float stop, int count) {
  std::vector<SkPoint> result;
  result.reserve((size_t)count + 1);
  for (int i = 0; i <= count; ++i) {
    const float t = (float)i / (float)count;
    const float angle = start + (stop - start) * t;
    result.push_back(
        {x + std::cos(angle) * radius, y - std::sin(angle) * radius});
  }
  return result;
}

Stroke closedSamples(std::span<const SkPoint> points, float spacing) {
  Stroke result;
  if (points.empty()) return result;
  for (size_t edge = 0; edge < points.size(); ++edge) {
    const Stroke side = brush::segment(
        points[edge], points[(edge + 1) % points.size()], spacing);
    result.insert(result.end(), side.begin(), side.end() - 1);
  }
  result.push_back(result.front());
  return result;
}

}  // namespace

Engine::Engine()
    : m_box(Box::stock()),
      m_massBrush(*m_box.find("crayon")),
      m_fields(stockFields()) {}

Engine::Engine(Box box) : m_box(std::move(box)) {
  const std::vector<std::string> names = m_box.names();
  if (!m_box.contains(m_selected) && !names.empty()) m_selected = names.front();
  m_massBrush = definition() ? *definition() : Brush{};
  m_fields = stockFields();
}

bool Engine::angleMode(Constant mode) {
  if (mode != RADIANS && mode != DEGREES) return false;
  m_angleMode = mode;
  return true;
}

bool Engine::add(std::string name, Brush brush) {
  return m_box.add(std::move(name), std::move(brush));
}

std::vector<std::string> Engine::box() const { return m_box.names(); }

void Engine::scaleBrushes(float factor) { m_box.scale(factor); }

bool Engine::pick(std::string_view name) {
  if (!m_box.contains(name)) return false;
  m_selected = name;
  return true;
}

bool Engine::set(std::string_view name, SkColor4f color, float weight) {
  if (!pick(name)) return false;
  m_color = color;
  m_weight = std::max(0.0f, weight);
  m_strokeActive = true;
  return true;
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

const Brush* Engine::definition() const { return m_box.find(m_selected); }

Brush Engine::current(bool devicePressure) const {
  const Brush* found = definition();
  if (!found) return {};
  Brush result = *found;
  result.color = m_color;
  result.width *= m_weight;
  result.scatter *= m_weight;
  if (devicePressure) {
    result.pressure = {1, 1, 1};
    result.pressure.variation.reset();
  }
  return result;
}

Brush Engine::brush() const { return current(); }

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
  m_fill.bleedAngle =
      angle ? std::optional<float>(m_angleMode == DEGREES ? radians(*angle)
                                                          : *angle)
            : std::nullopt;
}

void Engine::fillTexture(float texture, float border, bool scatter) {
  m_fill.texture = std::clamp(texture, 0.0f, 1.0f);
  m_fill.border = std::clamp(border, 0.0f, 1.0f);
  m_fill.scatter = scatter;
}

bool Engine::hatch(Hatch style) {
  if (!(style.spacing > 0.0f)) return false;
  if (m_angleMode == DEGREES) style.angle = radians(style.angle);
  m_hatch = style;
  return true;
}

void Engine::noHatch() {
  m_hatch.reset();
  m_hatchBrush.reset();
}

bool Engine::hatchStyle(std::string_view name, SkColor4f color, float weight) {
  const Brush* found = m_box.find(name);
  if (!found) return false;
  Brush tool = *found;
  tool.color = color;
  tool.width *= std::max(0.0f, weight);
  tool.scatter *= std::max(0.0f, weight);
  m_hatchBrush = std::move(tool);
  return true;
}

bool Engine::mass(std::string_view name, SkColor4f color, const Mass& style) {
  const Brush* found = m_box.find(name);
  if (!found) return false;
  m_massBrush = *found;
  m_massBrush.color = color;
  m_mass = style;
  return true;
}

void Engine::noMass() { m_mass.reset(); }

bool Engine::addField(std::string name, Field fieldValue, Constant units) {
  if (name.empty() || !fieldValue) return false;
  if (units != RADIANS && units != DEGREES) return false;
  if (units == DEGREES) {
    fieldValue = [source = std::move(fieldValue)](SkPoint point,
                                                  float seconds) {
      return radians(source(point, seconds));
    };
  }
  m_fields.insert_or_assign(std::move(name), std::move(fieldValue));
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
  if (!m_fields.contains(std::string(name))) return false;
  m_selectedField = name;
  return true;
}

void Engine::noField() { m_selectedField.clear(); }

bool Engine::refreshField(float seconds) {
  if (m_selectedField.empty() || !m_fields.contains(m_selectedField))
    return false;
  m_fieldSeconds = seconds;
  return true;
}

void Engine::wiggle(float amount) {
  m_fieldAmount = amount;
  m_selectedField = "hand";
}

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
    SkPathBuilder builder;
    builder.addRect(m_clip->region);
    const SkPath devicePath =
        builder.detach().makeTransform(*m_clip->transform);
    const SkMatrix current = canvas->getTotalMatrix();
    canvas->resetMatrix();
    canvas->clipPath(devicePath, SkClipOp::kIntersect, true);
    canvas->setMatrix(current);
    return;
  }
  const SkRect region = m_clip->region;
  pen.clip([&] {
    pen.rect(region.left(), region.top(), region.width(), region.height());
  });
}

void Engine::push() {
  m_states.push_back({m_selected, m_color, m_weight, m_strokeActive, m_fill,
                      m_fillActive, m_washColor, m_washOpacity, m_washActive,
                      m_hatch, m_hatchBrush, m_mass, m_massBrush,
                      m_selectedField, m_fieldSeconds, m_fieldAmount, m_clip,
                      m_angleMode});
}

bool Engine::pop() {
  if (m_states.empty()) return false;
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
  m_hatchBrush = std::move(state.hatchBrush);
  m_mass = state.mass;
  m_massBrush = std::move(state.massBrush);
  m_selectedField = std::move(state.selectedField);
  m_fieldSeconds = state.fieldSeconds;
  m_fieldAmount = state.fieldAmount;
  m_clip = state.clip;
  m_angleMode = state.angleMode;
  return true;
}

Stroke Engine::shaped(std::span<const Sample> path, bool applyField) const {
  Stroke result(path.begin(), path.end());
  if (!applyField || path.size() < 2 || m_selectedField.empty()) return result;
  const auto found = m_fields.find(m_selectedField);
  if (found == m_fields.end()) return result;
  result.front().position = path.front().position;
  for (size_t index = 1; index < path.size(); ++index) {
    const SkPoint sourceFrom = path[index - 1].position;
    const SkPoint sourceTo = path[index].position;
    const float dx = sourceTo.fX - sourceFrom.fX;
    const float dy = sourceTo.fY - sourceFrom.fY;
    const float length = std::hypot(dx, dy);
    const float base = length > 0.0f ? std::atan2(dy, dx) : 0.0f;
    const SkPoint from = result[index - 1].position;
    const float influence = found->second(from, m_fieldSeconds) * m_fieldAmount;
    result[index].position = {from.fX + std::cos(base + influence) * length,
                              from.fY + std::sin(base + influence) * length};
  }
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

bool Engine::paintStroke(Pen& pen, std::span<const Sample> path,
                         bool applyField) const {
  if (!hasStroke()) return false;
  const Stroke result = shaped(path, applyField);
  pen.push();
  applyClip(pen);
  brush::paint(pen, current(), result);
  pen.pop();
  return true;
}

bool Engine::paint(Pen& pen, std::span<const Sample> path) const {
  return paintStroke(pen, path, true);
}

std::vector<SkPoint> Engine::shapedPolygon(std::span<const SkPoint> points,
                                           bool applyField) const {
  std::vector<SkPoint> result(points.begin(), points.end());
  if (!applyField || m_selectedField.empty()) return result;
  const Stroke boundary = closedSamples(points, 5.0f);
  const Stroke warped = shaped(boundary, true);
  if (warped.empty()) return result;
  result.clear();
  result.reserve(warped.size() - 1);
  for (size_t index = 0; index + 1 < warped.size(); ++index)
    result.push_back(warped[index].position);
  return result;
}

bool Engine::line(Pen& pen, SkPoint from, SkPoint to, float startPressure,
                  float endPressure) const {
  if (!hasStroke() || from == to) return false;
  return paint(pen, brush::segment(from, to, current().spacing, startPressure,
                                   endPressure));
}

bool Engine::flowLine(Pen& pen, SkPoint start, float length,
                      float direction) const {
  if (!hasStroke() || !(length > 0.0f)) return false;
  if (m_angleMode == DEGREES) direction = radians(direction);
  const Brush tool = current();
  const Stroke path = brush::segment(start,
                                     {start.fX + std::cos(direction) * length,
                                      start.fY - std::sin(direction) * length},
                                     tool.spacing);
  return paintStroke(pen, path, true);
}

Plot Engine::spline(Pen& pen, std::span<const Sample> controls,
                    float curvature) const {
  const Stroke curve = brush::spline(
      controls, definition() ? definition()->spacing : 1.0f, curvature);
  Plot result = Plot::fromStroke(curve, PlotType::Segments);
  if (hasStroke()) paint(pen, curve);
  return result;
}

bool Engine::paintPolygon(Pen& pen, std::span<const SkPoint> points,
                          bool applyField, bool paintOutline) const {
  if (points.size() < 3) return false;
  const std::vector<SkPoint> shaped = shapedPolygon(points, applyField);

  pen.push();
  if (m_washActive) {
    SkColor4f color = m_washColor;
    color.fA *= m_washOpacity;
    pen.noStroke();
    pen.fill(color);
    pen.beginShape();
    for (SkPoint point : shaped) pen.vertex(point.fX, point.fY);
    pen.endShape(CLOSE);
  }
  if (m_fillActive) brush::wash(pen, m_fill, shaped);
  if (m_mass) brush::mass(pen, m_massBrush, shaped, *m_mass);
  if (m_hatch) {
    pen.push();
    applyClip(pen);
    brush::hatch(pen, m_hatchBrush.value_or(current()), shaped, *m_hatch);
    pen.pop();
  }
  if (paintOutline && hasStroke()) {
    Stroke outline;
    outline.reserve(shaped.size() + 1);
    for (SkPoint point : shaped) outline.push_back({point, 1.0f});
    outline.push_back(outline.front());
    paintStroke(pen, outline, false);
  }
  pen.pop();
  return m_washActive || m_fillActive || m_mass.has_value() ||
         m_hatch.has_value() || hasStroke();
}

Polygon Engine::polygon(Pen& pen, std::span<const SkPoint> points) const {
  Polygon result(std::vector<SkPoint>(points.begin(), points.end()));
  paintPolygon(pen, result.vertices, false);
  return result;
}

Polygon Engine::polygon(Pen& pen, const Polygon& stored) const {
  paintPolygon(pen, stored.vertices, false);
  return stored;
}

bool Engine::hatchArray(Pen& pen, std::span<const Polygon> polygons) const {
  if (!m_hatch) return false;
  pen.push();
  applyClip(pen);
  brush::hatchArray(pen, m_hatchBrush.value_or(current()), polygons, *m_hatch);
  pen.pop();
  return !polygons.empty();
}

bool Engine::hatchArray(Pen& pen, const Polygon& polygon) const {
  return hatchArray(pen, std::span<const Polygon>(&polygon, 1));
}

bool Engine::massArray(Pen& pen, std::span<const Polygon> polygons) const {
  if (!m_mass) return false;
  brush::massArray(pen, m_massBrush, polygons, *m_mass);
  return !polygons.empty();
}

bool Engine::massArray(Pen& pen, const Polygon& polygon) const {
  return massArray(pen, std::span<const Polygon>(&polygon, 1));
}

bool Engine::rect(Pen& pen, float x, float y, float width, float height) const {
  return rect(pen, x, y, width, height, RectMode::Corner);
}

bool Engine::rect(Pen& pen, float x, float y, float width, float height,
                  RectMode mode) const {
  if (mode == RectMode::Center) {
    x -= width * 0.5f;
    y -= height * 0.5f;
  } else if (mode == RectMode::Corners) {
    width -= x;
    height -= y;
  }
  const std::array<SkPoint, 4> points{
      {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}}};
  return paintPolygon(pen, points, true);
}

bool Engine::rect(Pen& pen, float x, float y, float width, float height,
                  float radius) const {
  radius = std::clamp(radius, 0.0f,
                      std::min(std::abs(width), std::abs(height)) * 0.5f);
  if (!(radius > 0.0f)) return rect(pen, x, y, width, height);
  std::vector<SkPoint> points;
  points.reserve(32);
  const std::array<SkPoint, 4> centers{{
      {x + width - radius, y + radius},
      {x + width - radius, y + height - radius},
      {x + radius, y + height - radius},
      {x + radius, y + radius},
  }};
  for (int corner = 0; corner < 4; ++corner) {
    const float start = (float)corner * HALF_PI - HALF_PI;
    for (int step = 0; step < 8; ++step) {
      const float angle = start + HALF_PI * (float)step / 7.0f;
      points.push_back({centers[corner].fX + std::cos(angle) * radius,
                        centers[corner].fY + std::sin(angle) * radius});
    }
  }
  return paintPolygon(pen, points, true);
}

PlacedPlot Engine::circle(Pen& pen, float x, float y, float radius) const {
  return circle(pen, x, y, radius, 0.0f);
}

PlacedPlot Engine::circle(Pen& pen, float x, float y, float radius,
                          float irregularity) const {
  if (!(radius > 0.0f)) return {Plot{}, {0, 0}};
  std::vector<SkPoint> points =
      circlePoints(x, y, std::max(0.0f, radius), 0.0f, TWO_PI, 96);
  irregularity = std::max(0.0f, irregularity);
  if (irregularity > 0.0f) {
    for (SkPoint& point : points) {
      const float dx = point.fX - x;
      const float dy = point.fY - y;
      const float scale =
          1.0f + pen.randomGaussian(0.0f, irregularity * 0.018f);
      point = {x + dx * scale, y + dy * scale};
    }
  }
  paintPolygon(pen, points, true);
  Stroke path;
  path.reserve(points.size());
  for (SkPoint point : points) path.push_back({point, 1.0f});
  return {Plot::fromStroke(path, PlotType::Segments), points.front()};
}

std::optional<Plot> Engine::arc(Pen& pen, float x, float y, float radius,
                                float start, float stop) const {
  if (m_angleMode == DEGREES) {
    start = radians(start);
    stop = radians(stop);
  }
  if (!(radius > 0.0f)) return std::nullopt;
  float sweep = std::fmod(stop - start, TWO_PI);
  if (sweep < 0.0f) sweep += TWO_PI;
  if (std::abs(sweep) < 0.000001f) return std::nullopt;
  stop = start + sweep;
  std::vector<SkPoint> points = circlePoints(x, y, radius, start, stop, 64);
  Stroke path;
  path.reserve(points.size());
  for (SkPoint point : points) path.push_back({point, 1.0f});
  paint(pen, path);
  return Plot::fromStroke(path, PlotType::Segments);
}

void Engine::beginShape(float curvature) {
  m_shape.clear();
  m_shapeCurvature = std::clamp(curvature, 0.0f, 1.0f);
}

void Engine::vertex(float x, float y, float pressure) {
  m_shape.push_back({{x, y}, std::max(0.0f, pressure)});
}

std::optional<Plot> Engine::endShape(Pen& pen, bool close) {
  if (m_shape.size() < 2) {
    m_shape.clear();
    return std::nullopt;
  }
  Stroke shapePath = brush::spline(
      m_shape, definition() ? definition()->spacing : 1.0f, m_shapeCurvature);
  Plot result = Plot::fromStroke(shapePath, PlotType::Segments);
  if (close) {
    if (shapePath.front().position != shapePath.back().position)
      shapePath.push_back(shapePath.front());
    std::vector<SkPoint> polygonPoints;
    polygonPoints.reserve(shapePath.size());
    for (const Sample& sample : shapePath)
      polygonPoints.push_back(sample.position);
    paintPolygon(pen, polygonPoints, true, false);
    paintStroke(pen, shapePath, true);
  } else {
    paint(pen, shapePath);
  }
  m_shape.clear();
  return result;
}

bool Engine::draw(Pen& pen, const Polygon& stored) const {
  if (!hasStroke() || stored.empty()) return false;
  Stroke outline;
  outline.reserve(stored.vertices.size() + 1);
  for (SkPoint point : stored.vertices) outline.push_back({point, 1.0f});
  outline.push_back(outline.front());
  return paintStroke(pen, outline, false);
}

bool Engine::fill(Pen& pen, const Polygon& stored) const {
  if (!m_fillActive || stored.empty()) return false;
  brush::wash(pen, m_fill, stored.vertices);
  return true;
}

bool Engine::wash(Pen& pen, const Polygon& stored) const {
  if (!m_washActive || stored.empty()) return false;
  SkColor4f color = m_washColor;
  color.fA *= m_washOpacity;
  pen.push();
  pen.noStroke();
  pen.fill(color);
  pen.beginShape();
  for (SkPoint point : stored.vertices) pen.vertex(point.fX, point.fY);
  pen.endShape(CLOSE);
  pen.pop();
  return true;
}

bool Engine::hatch(Pen& pen, const Polygon& stored) const {
  if (!m_hatch || stored.empty()) return false;
  pen.push();
  applyClip(pen);
  brush::hatch(pen, m_hatchBrush.value_or(current()), stored.vertices,
               *m_hatch);
  pen.pop();
  return true;
}

bool Engine::mass(Pen& pen, const Polygon& stored) const {
  if (!m_mass || stored.empty()) return false;
  brush::mass(pen, m_massBrush, stored.vertices, *m_mass);
  return true;
}

bool Engine::draw(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  if (!hasStroke() || !stored) return false;
  return paint(pen, stored.path({x, y}, current().spacing, 0.5f, scale));
}

bool Engine::fill(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  const Polygon geometry = stored.genPol(x, y, 1.0f, 0.5f, scale);
  return fill(pen, Polygon(shapedPolygon(geometry.vertices, true)));
}

bool Engine::wash(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  const Polygon geometry = stored.genPol(x, y, 1.0f, 0.5f, scale);
  return wash(pen, Polygon(shapedPolygon(geometry.vertices, true)));
}

bool Engine::hatch(Pen& pen, const Plot& stored, float x, float y,
                   float scale) const {
  const Polygon geometry = stored.genPol(x, y, 1.0f, 0.5f, scale);
  return hatch(pen, Polygon(shapedPolygon(geometry.vertices, true)));
}

bool Engine::mass(Pen& pen, const Plot& stored, float x, float y,
                  float scale) const {
  const Polygon geometry = stored.genPol(x, y, 1.0f, 0.5f, scale);
  return mass(pen, Polygon(shapedPolygon(geometry.vertices, true)));
}

Position Engine::position(float x, float y) const {
  const auto found = m_fields.find(m_selectedField);
  Field active;
  if (found != m_fields.end()) {
    active = [field = found->second, amount = m_fieldAmount](SkPoint point,
                                                             float seconds) {
      return field(point, seconds) * amount;
    };
  }
  return found == m_fields.end()
             ? Position(x, y, {}, 0.0f, m_angleMode)
             : Position(x, y, std::move(active), m_fieldSeconds, m_angleMode);
}

Position Engine::position(const Pen& pen, float x, float y) const {
  const auto found = m_fields.find(m_selectedField);
  Field active;
  if (found != m_fields.end()) {
    active = [field = found->second, amount = m_fieldAmount](SkPoint point,
                                                             float seconds) {
      return field(point, seconds) * amount;
    };
  }
  return Position(x, y, std::move(active), m_fieldSeconds, m_angleMode,
                  SkRect::MakeWH((float)pen.width, (float)pen.height));
}

void Engine::depositInput(Pen& pen, std::span<const Dab> sampled,
                          DepositOptions options) {
  if (!m_liveBrush || sampled.empty()) return;
  std::vector<Dab> marks(sampled.begin(), sampled.end());
  if (!m_selectedField.empty()) {
    const auto found = m_fields.find(m_selectedField);
    if (found != m_fields.end()) {
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
              found->second(shapedDab->position, m_fieldSeconds) *
              m_fieldAmount;
          dab.direction = base + influence;
          dab.position = {
              shapedDab->position.fX + std::cos(dab.direction) * length,
              shapedDab->position.fY + std::sin(dab.direction) * length};
        }
        source = unshaped;
        shapedDab = dab;
      }
      m_liveLastSourceDab = source;
    }
  }
  if (m_liveBrush->tip == Tip::Fibers && m_liveLastDab && !options.start)
    marks.insert(marks.begin(), *m_liveLastDab);

  pen.push();
  applyClip(pen);
  deposit(pen, *m_liveBrush, marks, options);
  pen.pop();
  m_liveLastDab = marks.back();
}

bool Engine::beginInput(Pen& pen, Input input) {
  if (!hasStroke()) return false;
  m_liveBrush = prepareStroke(pen, current(true));
  m_liveLastDab.reset();
  m_liveLastSourceDab.reset();
  const std::vector<Dab> sampled = m_sampler.begin(input);
  depositInput(pen, sampled, {.start = true, .end = false});
  return true;
}

bool Engine::moveInput(Pen& pen, Input input) {
  if (!hasStroke() || !m_sampler.active()) return false;
  if (!m_liveBrush) return false;
  const std::vector<Dab> sampled = m_sampler.move(input, m_liveBrush->spacing);
  depositInput(pen, sampled, {.start = false, .end = false});
  return true;
}

bool Engine::endInput(Pen& pen, Input input) {
  if (!hasStroke() || !m_sampler.active()) return false;
  if (!m_liveBrush) return false;
  const std::vector<Dab> sampled = m_sampler.end(input, m_liveBrush->spacing);
  depositInput(pen, sampled, {.start = false, .end = true});
  m_liveBrush.reset();
  m_liveLastDab.reset();
  m_liveLastSourceDab.reset();
  return true;
}

void Engine::cancelInput() {
  m_sampler.cancel();
  m_liveBrush.reset();
  m_liveLastDab.reset();
  m_liveLastSourceDab.reset();
}

void Engine::beginStroke(StrokeKind kind, SkPoint position) {
  m_strokeKind = kind;
  m_stroke.clear();
  m_stroke.push_back({position, 1.0f});
}

bool Engine::move(float angle, float length, float pressure) {
  if (!m_strokeKind || m_stroke.empty() || !(length >= 0.0f)) return false;
  if (m_angleMode == DEGREES) angle = radians(angle);
  const SkPoint from = m_stroke.back().position;
  m_stroke.push_back(
      {{from.fX + std::cos(angle) * length, from.fY - std::sin(angle) * length},
       std::max(0.0f, pressure)});
  return true;
}

Stroke Engine::endStroke(Pen& pen, float angle, float pressure) {
  if (!m_strokeKind || m_stroke.empty()) return {};
  if (m_angleMode == DEGREES) angle = radians(angle);
  m_stroke.back().pressure = std::max(0.0f, pressure);
  Stroke result =
      *m_strokeKind == StrokeKind::Curve
          ? brush::spline(m_stroke, definition() ? definition()->spacing : 1.0f,
                          0.65f)
          : m_stroke;
  if (result.size() >= 2 && *m_strokeKind == StrokeKind::Curve) {
    const SkPoint before = result[result.size() - 2].position;
    const SkPoint end = result.back().position;
    const float tail =
        std::min(std::hypot(end.fX - before.fX, end.fY - before.fY),
                 definition() ? definition()->spacing : 1.0f);
    result.back().position = {before.fX + std::cos(angle) * tail,
                              before.fY - std::sin(angle) * tail};
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
