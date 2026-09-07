/** @file
 * The Qt item: the properties QML binds, the scene and font controls it
 * offers, and the renderer it hands the render thread.
 */

#include "GalleryView.h"

#include <include/core/SkFontArguments.h>
#include <include/core/SkFontParameters.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/qt/SigilWeaveQt.h>

#include <QFont>
#include <QMouseEvent>
#include <QVariantMap>
#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "GalleryFallback.h"
#include "GalleryRenderer.h"
#include "SceneRegistry.h"

using namespace gallery;

namespace {

QString axisTagName(uint32_t tag) {
  QString name;
  name.reserve(4);
  for (unsigned byteIndex = 0; byteIndex < 4; ++byteIndex) {
    const unsigned shift = 24u - 8u * byteIndex;
    name.append(QChar(static_cast<char>((tag >> shift) & 0xffu)));
  }
  return name;
}

}  // namespace

GalleryView::GalleryView(QQuickItem* parent) : QQuickRhiItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton);
  m_timer.setInterval(16);
  connect(&m_timer, &QTimer::timeout, this, [this] { update(); });
  m_timer.start();
}

GalleryView::~GalleryView() = default;

QQuickRhiItemRenderer* GalleryView::createRenderer() {
  return new GalleryViewRenderer;
}

QStringList GalleryView::sceneNames() const {
  QStringList names;
  for (const SceneDescriptor& descriptor : sceneRegistry())
    names << descriptor.name;
  return names;
}

void GalleryView::setSceneIndex(int index) {
  if (index == m_sceneIndex || index < 0 ||
      index >= static_cast<int>(sceneRegistry().size()))
    return;
  m_sceneIndex = index;
  m_sceneText.clear();
  ++m_sceneParameterRevision;  // renderer re-pulls the new scene's values
  emit sceneIndexChanged();
  emit sceneTextChanged();
  emit sceneParameterValuesChanged();
  update();
}

void GalleryView::setAnimating(bool enabled) {
  if (enabled == m_animating) return;
  m_animating = enabled;
  if (enabled)
    m_timer.start();
  else
    m_timer.stop();
  emit animatingChanged();
  update();
}

namespace {

const SceneDescriptor* descriptorAt(int sceneIndex) {
  const auto& registry = sceneRegistry();
  if (sceneIndex < 0 || sceneIndex >= static_cast<int>(registry.size()))
    return nullptr;
  return &registry[static_cast<size_t>(sceneIndex)];
}

QVariantMap defaultParameterValues(const SceneDescriptor& descriptor) {
  QVariantMap values;
  for (const SceneParameter& parameter : descriptor.parameters)
    values.insert(parameter.id, parameter.defaultValue);
  return values;
}

}  // namespace

bool GalleryView::textEditable() const {
  const SceneDescriptor* descriptor = descriptorAt(m_sceneIndex);
  return descriptor && descriptor->textEditable;
}

QVariantList GalleryView::sceneParameters() const {
  QVariantList parameters;
  const SceneDescriptor* descriptor = descriptorAt(m_sceneIndex);
  if (!descriptor) return parameters;
  parameters.reserve(descriptor->parameters.size());
  for (const SceneParameter& parameter : descriptor->parameters) {
    QVariantMap map;
    map.insert(QStringLiteral("id"), parameter.id);
    map.insert(QStringLiteral("label"), parameter.label);
    map.insert(QStringLiteral("type"), [&] {
      switch (parameter.type) {
        case SceneParameter::Type::kBool:
          return QStringLiteral("bool");
        case SceneParameter::Type::kInt:
          return QStringLiteral("int");
        case SceneParameter::Type::kChoice:
          return QStringLiteral("choice");
        case SceneParameter::Type::kFloat:
          break;
      }
      return QStringLiteral("float");
    }());
    map.insert(QStringLiteral("defaultValue"), parameter.defaultValue);
    map.insert(QStringLiteral("minimum"), parameter.minimum);
    map.insert(QStringLiteral("maximum"), parameter.maximum);
    map.insert(QStringLiteral("suffix"), parameter.suffix);
    map.insert(QStringLiteral("choices"), parameter.choices);
    parameters.push_back(map);
  }
  return parameters;
}

QUrl GalleryView::sceneControlsQml() const {
  const SceneDescriptor* descriptor = descriptorAt(m_sceneIndex);
  return descriptor ? descriptor->controlsQml : QUrl();
}

const QVariantMap& GalleryView::parameterValuesForScene(int sceneIndex) const {
  auto values = m_sceneParameterValues.find(sceneIndex);
  if (values == m_sceneParameterValues.end()) {
    const SceneDescriptor* descriptor = descriptorAt(sceneIndex);
    values = m_sceneParameterValues.insert(
        sceneIndex,
        descriptor ? defaultParameterValues(*descriptor) : QVariantMap());
  }
  return *values;
}

QVariantMap GalleryView::sceneParameterValues() const {
  return parameterValuesForScene(m_sceneIndex);
}

void GalleryView::setSceneParameter(const QString& id, const QVariant& value) {
  const SceneDescriptor* descriptor = descriptorAt(m_sceneIndex);
  if (!descriptor) return;
  const auto parameter = std::find_if(
      descriptor->parameters.cbegin(), descriptor->parameters.cend(),
      [&](const SceneParameter& candidate) { return candidate.id == id; });
  if (parameter == descriptor->parameters.cend())
    return;  // only declared parameters are stored

  QVariant clampedValue = value;
  switch (parameter->type) {
    case SceneParameter::Type::kBool:
      clampedValue = value.toBool();
      break;
    case SceneParameter::Type::kFloat:
      clampedValue =
          std::clamp(value.toDouble(), parameter->minimum, parameter->maximum);
      break;
    case SceneParameter::Type::kInt:
      clampedValue = static_cast<int>(std::clamp<double>(
          value.toInt(), parameter->minimum, parameter->maximum));
      break;
    case SceneParameter::Type::kChoice:
      clampedValue = std::clamp(
          value.toInt(), 0, static_cast<int>(parameter->choices.size()) - 1);
      break;
  }

  QVariantMap& values =
      const_cast<QVariantMap&>(parameterValuesForScene(m_sceneIndex));
  if (values.value(id) == clampedValue) return;
  values.insert(id, clampedValue);
  ++m_sceneParameterRevision;
  emit sceneParameterValuesChanged();
  update();
}

void GalleryView::setSceneText(const QString& text) {
  if (text == m_sceneText) return;
  m_sceneText = text;
  emit sceneTextChanged();
  update();
}

void GalleryView::setFontFamily(const QString& family) {
  if (family == m_fontFamily) return;
  m_fontFamily = family;
  refreshFontAxes();
  emit fontFamilyChanged();
  update();
}

void GalleryView::setFontSize(qreal size) {
  const qreal clampedSize = std::clamp(size, 8.0, 200.0);
  if (clampedSize == m_fontSize) return;
  m_fontSize = clampedSize;
  emit fontSizeChanged();
  update();
}

QVariantList GalleryView::fontAxes() const {
  QVariantList axes;
  axes.reserve(static_cast<qsizetype>(m_fontAxes.size()));
  for (const FontAxis& axis : m_fontAxes) {
    QVariantMap values;
    values.insert(QStringLiteral("tag"), axis.tagName);
    values.insert(QStringLiteral("minimum"), axis.minimum);
    values.insert(QStringLiteral("defaultValue"), axis.defaultValue);
    values.insert(QStringLiteral("maximum"), axis.maximum);
    values.insert(QStringLiteral("hidden"), axis.hidden);
    axes.push_back(values);
  }
  return axes;
}

QVariantMap GalleryView::fontAxisValues() const {
  QVariantMap values;
  for (const FontAxis& axis : m_fontAxes)
    values.insert(axis.tagName, axis.value);
  return values;
}

void GalleryView::setFontAxisValue(const QString& tag, qreal value) {
  for (FontAxis& axis : m_fontAxes) {
    if (axis.tagName != tag) continue;
    const float clampedValue =
        std::clamp(static_cast<float>(value), axis.minimum, axis.maximum);
    if (clampedValue == axis.value) return;
    axis.value = clampedValue;
    ++m_fontAxesRevision;
    emit fontAxisValuesChanged();
    update();
    return;
  }
}

void GalleryView::refreshFontAxes() {
  std::vector<FontAxis> axes;
  if (!m_fontFamily.isEmpty()) {
    // Axis discovery is GUI-side so QML updates as soon as the family changes.
    // Rendering resolves the same family independently on the render thread.
    const sk_sp<SkFontMgr> fontManager =
        sigil::weave::ports::systemFontManager();
    sk_sp<SkTypeface> typeface =
        resolveGalleryTypeface(fontManager.get(), m_fontFamily);
    const int axisCount =
        typeface ? typeface->getVariationDesignParameters({}) : 0;
    if (axisCount > 0) {
      std::vector<SkFontParameters::Variation::Axis> parameters(
          static_cast<size_t>(axisCount));
      std::vector<SkFontArguments::VariationPosition::Coordinate> position(
          static_cast<size_t>(axisCount));
      const bool haveParameters =
          typeface->getVariationDesignParameters(
              {parameters.data(), parameters.size()}) == axisCount;
      const bool havePosition =
          typeface->getVariationDesignPosition(
              {position.data(), position.size()}) == axisCount;
      if (haveParameters) {
        axes.reserve(parameters.size());
        for (const SkFontParameters::Variation::Axis& parameter : parameters) {
          float value = parameter.def;
          if (havePosition) {
            const auto coordinate = std::find_if(
                position.begin(), position.end(), [&](const auto& candidate) {
                  return candidate.axis == parameter.tag;
                });
            if (coordinate != position.end()) value = coordinate->value;
          }
          axes.push_back({parameter.tag, axisTagName(parameter.tag),
                          parameter.min, parameter.def, parameter.max,
                          std::clamp(value, parameter.min, parameter.max),
                          parameter.isHidden()});
        }
      }
    }
  }
  m_fontAxes = std::move(axes);
  ++m_fontAxesRevision;
  emit fontAxesChanged();
  emit fontAxisValuesChanged();
  update();
}

void GalleryView::setAlignmentIndex(int index) {
  index = std::clamp(index, 0, 3);
  if (index == m_alignmentIndex) return;
  m_alignmentIndex = index;
  emit alignmentIndexChanged();
  update();
}

void GalleryView::setLineBreakStrategyIndex(int index) {
  index = std::clamp(index, 0, 1);
  if (index == m_lineBreakStrategyIndex) return;
  m_lineBreakStrategyIndex = index;
  emit lineBreakStrategyIndexChanged();
  update();
}

bool GalleryView::gpuAvailable() const {
#ifdef TEXTFLOW_GALLERY_GPU
  return true;
#else
  return false;
#endif
}

void GalleryView::setGpu(bool enabled) {
  if (enabled == m_gpu) return;
  m_gpu = enabled;
  emit gpuChanged();
  update();
}

void GalleryView::mousePressEvent(QMouseEvent* event) {
  m_pendingClicks.push_back({static_cast<float>(event->position().x()),
                             static_cast<float>(event->position().y())});
  event->accept();
  update();
}
