/** @file
 * The layer stack: a layer's settings and its revision, and the stack
 * that composites its layers bottom to top into a buffer of its own.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkLumaColorFilter.h>
#include <sigildraw/Math.h>
#include <sigildraw/Stack.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "PenInternal.h"

namespace sigil::draw {
namespace {

float canvasDimension(float value) {
  if (!std::isfinite(value))
    throw std::invalid_argument("Stack dimensions must be finite");
  return std::max(1.0f, value);
}

bool finite(SkPoint point) {
  return std::isfinite(point.fX) && std::isfinite(point.fY);
}

}  // namespace

// ---- the placement ----------------------------------------------------------

SkMatrix LayerTransform::matrix() const {
  // The same concatenations the pen's verbs make on its canvas, in the
  // same order and the same precision, so a placement and the pen's own
  // matrix agree to the last bit.
  SkM44 matrix = SkM44::Translate(origin.fX + translation.fX,
                                  origin.fY + translation.fY);
  matrix.preConcat(SkM44(SkMatrix::RotateDeg(degrees(rotation))));
  matrix.preConcat(SkM44(SkMatrix::Skew(std::tan(shear.fX), 0.0f)));
  matrix.preConcat(SkM44(SkMatrix::Skew(0.0f, std::tan(shear.fY))));
  matrix.preScale(scale.fX, scale.fY);
  matrix.preTranslate(-origin.fX, -origin.fY);
  return matrix.asM33();
}

// ---- the layer --------------------------------------------------------------

Layer::Layer(Stack& stack, std::string name, float width, float height)
    : m_stack(&stack), m_name(std::move(name)), m_buffer(width, height) {}

Pen& Layer::begin(Pen& host) {
  touch();
  return m_buffer.begin(host);
}

void Layer::end() {
  m_buffer.end();
  touch();
}

void Layer::blendMode(Constant mode) {
  if (mode == m_blend) return;
  m_blend = mode;
  touch();
}

void Layer::opacity(float amount) {
  if (!std::isfinite(amount))
    throw std::invalid_argument("Layer opacity must be finite");
  const float clamped = std::clamp(amount, 0.0f, 1.0f);
  if (clamped == m_opacity) return;
  m_opacity = clamped;
  touch();
}

void Layer::transform(const LayerTransform& placement) {
  if (!finite(placement.origin) || !finite(placement.translation) ||
      !finite(placement.scale) || !finite(placement.shear) ||
      !std::isfinite(placement.rotation))
    throw std::invalid_argument("Layer transform must be finite");
  if (placement == m_transform) return;
  m_transform = placement;
  touch();
}

void Layer::mask(const Layer& source, MaskChannel channel) {
  if (&source == this)
    throw std::invalid_argument("A layer cannot mask itself");
  if (source.m_stack != m_stack)
    throw std::invalid_argument("A mask is a layer of the same stack");
  if (m_mask == &source && m_maskChannel == channel) return;
  m_mask = &source;
  m_maskChannel = channel;
  touch();
}

void Layer::noMask() {
  if (!m_mask) return;
  m_mask = nullptr;
  touch();
}

void Layer::clip(std::function<void(Pen&)> shape, ClipOptions options) {
  m_clip = std::move(shape);
  m_clipOptions = options;
  touch();
}

void Layer::noClip() {
  if (!m_clip) return;
  m_clip = nullptr;
  touch();
}

void Layer::show() {
  if (m_visible) return;
  m_visible = true;
  touch();
}

void Layer::hide() {
  if (!m_visible) return;
  m_visible = false;
  touch();
}

// ---- the stack --------------------------------------------------------------

Stack::Stack(float width, float height)
    : m_width(canvasDimension(width)),
      m_height(canvasDimension(height)),
      m_composite(m_width, m_height) {}

Stack::~Stack() = default;

Layer& Stack::layer(std::string_view name) {
  if (Layer* existing = find(name)) return *existing;
  auto made = std::unique_ptr<Layer>(
      new Layer(*this, std::string(name), m_width, m_height));
  made->m_buffer.setDensityFloor(m_densityFloor);
  m_layers.push_back(std::move(made));
  ++m_structure;
  return *m_layers.back();
}

Layer* Stack::find(std::string_view name) {
  for (const auto& layer : m_layers)
    if (layer->m_name == name) return layer.get();
  return nullptr;
}

const Layer* Stack::find(std::string_view name) const {
  for (const auto& layer : m_layers)
    if (layer->m_name == name) return layer.get();
  return nullptr;
}

bool Stack::remove(std::string_view name) {
  const auto found =
      std::find_if(m_layers.begin(), m_layers.end(),
                   [&](const auto& layer) { return layer->m_name == name; });
  if (found == m_layers.end()) return false;
  const Layer* removed = found->get();
  for (const auto& layer : m_layers)
    if (layer->m_mask == removed) layer->noMask();
  m_layers.erase(found);
  ++m_structure;
  return true;
}

bool Stack::reorder(std::string_view name, std::size_t position) {
  const auto found =
      std::find_if(m_layers.begin(), m_layers.end(),
                   [&](const auto& layer) { return layer->m_name == name; });
  if (found == m_layers.end()) return false;
  const std::size_t from = static_cast<std::size_t>(found - m_layers.begin());
  const std::size_t to = std::min(position, m_layers.size() - 1);
  if (from == to) return true;
  std::unique_ptr<Layer> moving = std::move(*found);
  m_layers.erase(found);
  m_layers.insert(m_layers.begin() + static_cast<std::ptrdiff_t>(to),
                  std::move(moving));
  ++m_structure;
  return true;
}

std::vector<std::string> Stack::names() const {
  std::vector<std::string> names;
  names.reserve(m_layers.size());
  for (const auto& layer : m_layers) names.push_back(layer->m_name);
  return names;
}

void Stack::resize(float width, float height) {
  const float w = canvasDimension(width);
  const float h = canvasDimension(height);
  if (w == m_width && h == m_height) return;
  m_width = w;
  m_height = h;
  for (const auto& layer : m_layers) layer->m_buffer.resize(w, h);
  m_composite.resize(w, h);
  ++m_structure;
}

void Stack::setDensityFloor(float devicePixelsPerUnit) {
  m_composite.setDensityFloor(devicePixelsPerUnit);
  for (const auto& layer : m_layers)
    layer->m_buffer.setDensityFloor(devicePixelsPerUnit);
  m_densityFloor = std::max(0.0f, devicePixelsPerUnit);
}

bool Stack::moved(float density) const {
  if (!m_made || m_structure != m_compositedStructure ||
      density != m_compositedDensity)
    return true;
  return std::any_of(m_layers.begin(), m_layers.end(), [](const auto& layer) {
    return layer->m_revision != layer->m_compositedRevision;
  });
}

void Stack::drawLayer(Pen& pen, const Layer& layer) {
  const sk_sp<SkImage> pixels = layer.image();
  if (!pixels) return;
  SkCanvas& canvas = *pen.canvas();
  const SkRect bounds = SkRect::MakeWH(m_width, m_height);
  const SkSamplingOptions sampling = detail::samplingFor(true);

  // How the layer meets what lies beneath it: its blend and its opacity,
  // carried by the image's own paint, or by the isolated group's when a
  // mask has to cut the layer before it meets anything.
  SkPaint meeting;
  detail::setBlend(meeting, layer.m_blend);
  meeting.setAlphaf(layer.m_opacity);

  pen.push();
  if (layer.m_clip) {
    const auto& shape = layer.m_clip;
    pen.clip([&] { shape(pen); }, layer.m_clipOptions);
  }
  const Layer* source = layer.m_mask;
  if (!source) {
    SkAutoCanvasRestore placed(&canvas, true);
    canvas.concat(layer.m_transform.matrix());
    canvas.drawImageRect(pixels, bounds, sampling, &meeting);
  } else {
    // THE MASK CUTS THE LAYER IN A GROUP OF ITS OWN, and the group meets
    // the layers beneath under the layer's blend and opacity: cutting in
    // place would cut what lies beneath as well.
    canvas.saveLayer(nullptr, &meeting);
    {
      SkAutoCanvasRestore placed(&canvas, true);
      canvas.concat(layer.m_transform.matrix());
      canvas.drawImageRect(pixels, bounds, sampling, nullptr);
    }
    // The mask's pixels as a shader that is clear beyond its own
    // rectangle, laid over the whole group: where the mask has no ink,
    // inside its rectangle or outside it, the layer is cut away.
    SkPaint cut;
    cut.setBlendMode(SkBlendMode::kDstIn);
    if (layer.m_maskChannel == MaskChannel::Luminance)
      cut.setColorFilter(SkLumaColorFilter::Make());
    if (const sk_sp<SkImage> stencil = source->image()) {
      SkMatrix placement = source->m_transform.matrix();
      placement.preScale(m_width / static_cast<float>(stencil->width()),
                         m_height / static_cast<float>(stencil->height()));
      cut.setShader(stencil->makeShader(SkTileMode::kDecal,
                                        SkTileMode::kDecal, sampling,
                                        &placement));
    } else {
      // A mask that was never drawn has no ink anywhere.
      cut.setColor(SK_ColorTRANSPARENT);
    }
    canvas.drawPaint(cut);
    canvas.restore();
  }
  pen.pop();
}

void Stack::remake(Pen& host) {
  Pen& pen = m_composite.begin(host);
  // A clip shape is the caller's code and may throw: the frame is ended
  // before the error goes on, as a bake ends its own, and the pen's end
  // closes whatever push the layer had open. Nothing is marked made, so
  // the next composite makes it again.
  try {
    pen.clear();
    for (const auto& layer : m_layers)
      if (layer->m_visible) drawLayer(pen, *layer);
  } catch (...) {
    m_composite.end();
    throw;
  }
  m_composite.end();
  for (const auto& layer : m_layers)
    layer->m_compositedRevision = layer->m_revision;
  m_compositedStructure = m_structure;
  m_made = true;
  ++m_revision;
}

sk_sp<SkImage> Stack::image(Pen& host) {
  const float density = std::max(host.contentScale(), m_densityFloor);
  if (moved(density)) {
    remake(host);
    m_compositedDensity = density;
  }
  return m_composite.image();
}

void Stack::composite(Pen& pen, float x, float y) {
  (void)image(pen);
  pen.image(m_composite, x, y);
}

}  // namespace sigil::draw
