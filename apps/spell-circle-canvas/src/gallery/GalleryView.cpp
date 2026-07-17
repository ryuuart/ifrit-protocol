#include "GalleryView.h"
#include "SceneRegistry.h"

#include <textflowqt/TextFlowQt.h>

#ifdef TEXTFLOW_GALLERY_GPU
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#endif

#include <include/core/SkCanvas.h>
#include <include/core/SkFontArguments.h>
#include <include/core/SkFontParameters.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <textflow/ports/SystemFontManager.h>

#include <QFont>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QVariantMap>
#include <rhi/qrhi.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace gallery;

namespace {

QString axisTagName(uint32_t tag) {
  QString name;
  name.reserve(4);
  for (int shift = 24; shift >= 0; shift -= 8)
    name.append(QChar(static_cast<char>((tag >> shift) & 0xff)));
  return name;
}

sk_sp<SkTypeface> resolveGalleryTypeface(SkFontMgr *fontManager,
                                         const QString &family) {
  if (!fontManager || family.isEmpty())
    return nullptr;
  return textflowqt::toSkTypeface(fontManager, QFont(family));
}

constexpr std::string_view kNotoSerifPrefix = "Noto Serif";
constexpr std::string_view kNotoCuneiformFamily = "Noto Sans Cuneiform";
constexpr const char *kSystemNotoCuneiformPath =
    "/System/Library/Fonts/Supplemental/NotoSansCuneiform-Regular.ttf";

bool isCuneiform(int32_t codePoint) {
  return codePoint >= 0x12000 && codePoint <= 0x1254F;
}

bool startsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

std::string typefaceFamily(const SkTypeface &typeface) {
  SkString family;
  typeface.getFamilyName(&family);
  return {family.c_str(), family.size()};
}

std::string_view preferredCjkSerif(std::string_view languageTag) {
  if (startsWith(languageTag, "ja"))
    return "Noto Serif JP";
  if (startsWith(languageTag, "ko"))
    return "Noto Serif KR";
  if (startsWith(languageTag, "zh-Hant") || startsWith(languageTag, "zh-TW"))
    return "Noto Serif TC";
  if (startsWith(languageTag, "zh-HK"))
    return "Noto Serif HK";
  if (startsWith(languageTag, "zh"))
    return "Noto Serif SC";
  return {};
}

sk_sp<SkTypeface> resolvePlatformFallback(SkFontMgr &fontManager,
                                          const SkTypeface &primaryTypeface,
                                          int32_t codePoint,
                                          std::string_view languageTag) {
  const std::string language(languageTag);
  const char *languageTags[] = {language.c_str()};
  return fontManager.matchFamilyStyleCharacter(
      nullptr, primaryTypeface.fontStyle(),
      language.empty() ? nullptr : languageTags, language.empty() ? 0 : 1,
      codePoint);
}

textflow::FontContext::FallbackResolver
makeGalleryFallbackResolver(SkFontMgr &fontManager) {
  std::vector<std::string> serifFamilies;
  sk_sp<SkTypeface> cuneiformTypeface =
      fontManager.makeFromFile(kSystemNotoCuneiformPath);
  const int familyCount = fontManager.countFamilies();
  serifFamilies.reserve(static_cast<size_t>(std::max(0, familyCount)));
  for (int familyIndex = 0; familyIndex < familyCount; ++familyIndex) {
    SkString family;
    fontManager.getFamilyName(familyIndex, &family);
    const std::string_view familyName(family.c_str(), family.size());
    if (startsWith(familyName, kNotoSerifPrefix))
      serifFamilies.emplace_back(familyName);
    else if (!cuneiformTypeface && familyName == kNotoCuneiformFamily)
      cuneiformTypeface = fontManager.matchFamilyStyle(
          kNotoCuneiformFamily.data(), SkFontStyle());
  }
  std::sort(serifFamilies.begin(), serifFamilies.end());
  serifFamilies.erase(std::unique(serifFamilies.begin(), serifFamilies.end()),
                      serifFamilies.end());

  return [serifFamilies = std::move(serifFamilies),
          cuneiformTypeface = std::move(cuneiformTypeface)](
             SkFontMgr &manager, const SkTypeface &primaryTypeface,
             int32_t codePoint,
             std::string_view languageTag) -> sk_sp<SkTypeface> {
    // macOS ships Noto Sans Cuneiform, but CoreText can resolve its character
    // map and rasterizer from different copies when a newer user-installed
    // font has the same PostScript name. Loading the system file directly
    // keeps HarfBuzz's glyph IDs and Skia's outlines from the same face.
    if (isCuneiform(codePoint) && cuneiformTypeface &&
        cuneiformTypeface->unicharToGlyph(codePoint) != 0)
      return cuneiformTypeface;

    const std::string primaryFamily = typefaceFamily(primaryTypeface);
    if (!startsWith(primaryFamily, kNotoSerifPrefix))
      return resolvePlatformFallback(manager, primaryTypeface, codePoint,
                                     languageTag);

    auto tryFamily = [&](std::string_view familyName) -> sk_sp<SkTypeface> {
      if (familyName.empty() || familyName == primaryFamily)
        return nullptr;
      const std::string terminatedFamilyName(familyName);
      sk_sp<SkTypeface> candidate = manager.matchFamilyStyle(
          terminatedFamilyName.c_str(), primaryTypeface.fontStyle());
      if (candidate && candidate->unicharToGlyph(codePoint) != 0)
        return candidate;
      return nullptr;
    };

    const std::string_view preferredFamily = preferredCjkSerif(languageTag);
    if (sk_sp<SkTypeface> preferred = tryFamily(preferredFamily))
      return preferred;
    for (const std::string &family : serifFamilies) {
      if (family != preferredFamily) {
        if (sk_sp<SkTypeface> candidate = tryFamily(family))
          return candidate;
      }
    }
    return resolvePlatformFallback(manager, primaryTypeface, codePoint,
                                   languageTag);
  };
}

} // namespace

// ── Render-thread side ─────────────────────────────────────────────────────

class GalleryViewRenderer : public QQuickRhiItemRenderer {
public:
  void initialize(QRhiCommandBuffer *commandBuffer) override;
  void synchronize(QQuickRhiItem *item) override;
  void render(QRhiCommandBuffer *commandBuffer) override;

private:
  struct FontCoordinate {
    uint32_t tag = 0;
    float value = 0;
    bool operator==(const FontCoordinate &) const = default;
  };

  /// Draws the selected scene in logical coordinates onto an Skia canvas.
  void renderScene(SkCanvas *canvas, float devicePixelRatio, QSize logicalSize);

  std::vector<std::unique_ptr<Scene>> m_scenes;
  std::unique_ptr<textflow::FontContext> m_fontContext;
#ifdef TEXTFLOW_GALLERY_GPU
  std::unique_ptr<SkiaGraphiteContext> m_graphiteContext;
  bool m_graphiteInitializationAttempted = false;
#endif
  std::vector<uint32_t> m_rasterPixels; // CPU fallback framebuffer.

  SceneParams m_sceneParameters;
  uint64_t m_sceneParameterRevision = std::numeric_limits<uint64_t>::max();
  int m_lastSyncedSceneIndex = -1;
  QString m_sceneText;
  QString m_fontFamily;
  std::vector<FontCoordinate> m_fontCoordinates;
  uint64_t m_fontAxesRevision = std::numeric_limits<uint64_t>::max();
  bool m_typefaceDirty = true;
  int m_sceneIndex = 0;
  bool m_animating = true;
  bool m_useGpuBackend = true;

  // The resolved typeface is rebuilt only when the GUI-side family/axis
  // revision changes. Stable frames do one integer comparison in synchronize.
  sk_sp<SkTypeface> m_resolvedTypeface;
  std::vector<SkPoint> m_pendingClicks;
  QSize m_logicalSize;

  QElapsedTimer m_clock;
  double m_pausedSeconds = 0;
  int m_frameNumber = 0;
  int m_lastSceneIndex = -1;

  // Stats accumulated in render(), copied back in the next synchronize().
  double m_layoutMicrosecondsAverage = 0;
  double m_recordMicrosecondsAverage = 0;
  double m_submitMicrosecondsAverage = 0;
  double m_framesPerSecondAverage = 0;
  uint64_t m_reshapedWordCount = 0;
  int m_runCount = 0;
  int m_glyphCount = 0;
  QElapsedTimer m_interFrameTimer;
  int m_statisticsFrameCount = 0;
  bool m_statsDirty = false;
};

void GalleryViewRenderer::initialize(QRhiCommandBuffer * /*commandBuffer*/) {
#ifdef TEXTFLOW_GALLERY_GPU
  if (!m_graphiteInitializationAttempted) {
    m_graphiteInitializationAttempted = true;
    // Same pattern as the SpellCircle app: Graphite built on Qt's own Metal
    // device/queue, so its submissions order before Qt's scene-graph pass.
    m_graphiteContext = SkiaGraphiteContext::create(rhi());
  }
#endif
}

void GalleryViewRenderer::synchronize(QQuickRhiItem *item) {
  auto *view = static_cast<GalleryView *>(item);
  if (m_scenes.empty()) {
    m_scenes.reserve(sceneRegistry().size());
    for (const SceneDescriptor &descriptor : sceneRegistry())
      m_scenes.push_back(descriptor.make());
  }

  m_sceneIndex = view->m_sceneIndex;
  if (m_animating != view->m_animating) {
    if (m_animating && m_clock.isValid())
      m_pausedSeconds += m_clock.elapsed() / 1000.0;
    m_clock.restart();
    m_animating = view->m_animating;
  }
  m_useGpuBackend = view->m_gpu;
  m_sceneText = view->m_sceneText;
  if (m_fontFamily != view->m_fontFamily) {
    m_fontFamily = view->m_fontFamily;
    m_typefaceDirty = true;
  }
  if (m_fontAxesRevision != view->m_fontAxesRevision) {
    m_fontAxesRevision = view->m_fontAxesRevision;
    m_fontCoordinates.clear();
    m_fontCoordinates.reserve(view->m_fontAxes.size());
    for (const GalleryView::FontAxis &axis : view->m_fontAxes)
      m_fontCoordinates.push_back({axis.tag, axis.value});
    m_typefaceDirty = true;
  }
  m_sceneParameters.text = m_sceneText;
  m_sceneParameters.fontSize = static_cast<float>(view->m_fontSize);
  m_sceneParameters.alignment = static_cast<textflow::TextAlignment>(
      std::clamp(view->m_alignmentIndex, 0, 3));
  m_sceneParameters.lineBreakStrategy =
      static_cast<textflow::LineBreakStrategy>(
          std::clamp(view->m_lineBreakStrategyIndex, 0, 1));
  // Scene-declared parameter values: copied only when they actually changed
  // (revision bumped by setSceneParameter / scene switch).
  if (m_sceneParameterRevision != view->m_sceneParameterRevision ||
      m_sceneIndex != m_lastSyncedSceneIndex) {
    m_sceneParameterRevision = view->m_sceneParameterRevision;
    m_lastSyncedSceneIndex = m_sceneIndex;
    m_sceneParameters.values = view->parameterValuesForScene(m_sceneIndex);
  }
  m_pendingClicks.insert(m_pendingClicks.end(), view->m_pendingClicks.begin(),
                         view->m_pendingClicks.end());
  view->m_pendingClicks.clear();
  m_logicalSize =
      QSize(static_cast<int>(view->width()), static_cast<int>(view->height()));

  // Ship last frame's stats back (GUI thread is blocked right now).
  if (m_statsDirty) {
    m_statsDirty = false;
    const char *mode =
#ifdef TEXTFLOW_GALLERY_GPU
        (m_graphiteContext && m_useGpuBackend) ? "Graphite GPU" : "CPU raster";
#else
        "CPU raster";
#endif
    view->m_stats =
        QStringLiteral(
            "%1 · %2 fps · layout %3 µs · record %4 ms · submit %5 ms\n"
            "%6 runs · %7 reshaped/frame%8")
            .arg(QLatin1String(mode))
            .arg(m_framesPerSecondAverage, 0, 'f', 0)
            .arg(m_layoutMicrosecondsAverage, 0, 'f', 0)
            .arg(m_recordMicrosecondsAverage / 1000.0, 0, 'f', 2)
            .arg(m_submitMicrosecondsAverage / 1000.0, 0, 'f', 2)
            .arg(m_runCount)
            .arg(m_reshapedWordCount)
            .arg(m_glyphCount > 0
                     ? QStringLiteral(" · %1 letters").arg(m_glyphCount)
                     : QString());
    QMetaObject::invokeMethod(view, &GalleryView::statsChanged,
                              Qt::QueuedConnection);
  }
}

void GalleryViewRenderer::renderScene(SkCanvas *canvas, float devicePixelRatio,
                                      QSize logicalSize) {
  using Clock = std::chrono::steady_clock;
  if (!m_fontContext) {
    sk_sp<SkFontMgr> fontManager = textflow::ports::systemFontManager();
    m_fontContext = std::make_unique<textflow::FontContext>(
        fontManager, nullptr, makeGalleryFallbackResolver(*fontManager));
  }

  if (m_sceneIndex != m_lastSceneIndex) {
    m_lastSceneIndex = m_sceneIndex;
    m_clock.restart();
    m_pausedSeconds = 0;
    m_frameNumber = 0;
  }
  if (!m_clock.isValid())
    m_clock.start();

  Scene *scene =
      m_scenes[static_cast<size_t>(std::clamp<int>(
                   m_sceneIndex, 0, static_cast<int>(m_scenes.size()) - 1))]
          .get();

  // Font family resolves on this thread (owns the FontContext). Every axis
  // discovered from the selected family is cloned onto the typeface; shaping
  // follows because recordForTypeface mirrors the clone's complete design
  // position into HarfBuzz.
  if (m_fontFamily.isEmpty()) {
    if (m_typefaceDirty) {
      m_resolvedTypeface.reset();
      m_typefaceDirty = false;
    }
    m_sceneParameters.typeface = nullptr;
  } else {
    if (m_typefaceDirty) {
      sk_sp<SkTypeface> typeface =
          resolveGalleryTypeface(m_fontContext->fontManager(), m_fontFamily);
      if (typeface && !m_fontCoordinates.empty()) {
        std::vector<SkFontArguments::VariationPosition::Coordinate> coordinates;
        coordinates.reserve(m_fontCoordinates.size());
        for (const FontCoordinate &coordinate : m_fontCoordinates)
          coordinates.push_back({coordinate.tag, coordinate.value});
        SkFontArguments fontArguments;
        fontArguments.setVariationDesignPosition(
            {coordinates.data(), static_cast<int>(coordinates.size())});
        if (sk_sp<SkTypeface> variedTypeface =
                typeface->makeClone(fontArguments))
          typeface = std::move(variedTypeface);
      }
      m_resolvedTypeface = std::move(typeface);
      m_typefaceDirty = false;
    }
    m_sceneParameters.typeface = m_resolvedTypeface;
  }

  for (const SkPoint &click : m_pendingClicks)
    scene->pointerPress(click);
  m_pendingClicks.clear();

  canvas->save();
  canvas->scale(devicePixelRatio, devicePixelRatio);
  const double elapsedSeconds =
      m_pausedSeconds + (m_animating ? m_clock.elapsed() / 1000.0 : 0.0);

  const uint64_t shapeCallsBefore = m_fontContext->stats().shapeCalls;
  const auto recordingStartTime = Clock::now();
  const FrameStats frameStatistics = scene->render(
      canvas, {logicalSize.width(), logicalSize.height()}, elapsedSeconds,
      m_frameNumber, m_sceneParameters, *m_fontContext);
  const double recordMicroseconds = std::chrono::duration<double, std::micro>(
                                        Clock::now() - recordingStartTime)
                                        .count();
  canvas->restore();

  if (m_animating)
    m_frameNumber++;

  m_reshapedWordCount = m_fontContext->stats().shapeCalls - shapeCallsBefore;
  m_runCount = frameStatistics.runCount;
  m_glyphCount = frameStatistics.glyphCount;
  m_layoutMicrosecondsAverage =
      m_layoutMicrosecondsAverage == 0
          ? frameStatistics.layoutMicroseconds
          : m_layoutMicrosecondsAverage * 0.95 +
                frameStatistics.layoutMicroseconds * 0.05;
  m_recordMicrosecondsAverage =
      m_recordMicrosecondsAverage == 0
          ? recordMicroseconds
          : m_recordMicrosecondsAverage * 0.95 + recordMicroseconds * 0.05;
  if (m_animating && m_interFrameTimer.isValid()) {
    const double elapsedMilliseconds =
        static_cast<double>(m_interFrameTimer.nsecsElapsed()) / 1e6;
    if (elapsedMilliseconds > 0.1) {
      const double framesPerSecond = 1000.0 / elapsedMilliseconds;
      m_framesPerSecondAverage =
          m_framesPerSecondAverage == 0
              ? framesPerSecond
              : m_framesPerSecondAverage * 0.95 + framesPerSecond * 0.05;
    }
  }
  m_interFrameTimer.restart();
  if (++m_statisticsFrameCount % 15 == 0)
    m_statsDirty = true;
}

void GalleryViewRenderer::render(QRhiCommandBuffer *commandBuffer) {
  using Clock = std::chrono::steady_clock;
  QRhiTexture *texture = colorTexture();
  if (!texture || m_scenes.empty() || m_logicalSize.width() < 8 ||
      m_logicalSize.height() < 8)
    return;
  const QSize pixelSize = texture->pixelSize();
  const float devicePixelRatio = static_cast<float>(pixelSize.width()) /
                                 static_cast<float>(m_logicalSize.width());

#ifdef TEXTFLOW_GALLERY_GPU
  if (m_graphiteContext && m_useGpuBackend) {
    // GPU: record straight into the item's texture, submit asynchronously —
    // Qt's scene-graph pass on the same Metal queue orders after it.
    SkiaOffscreenSurface surface(*m_graphiteContext, texture, pixelSize);
    if (SkCanvas *canvas = surface.canvas()) {
      renderScene(canvas, devicePixelRatio, m_logicalSize);
      const auto submissionStartTime = Clock::now();
      surface.submit();
      const double submitMicroseconds =
          std::chrono::duration<double, std::micro>(Clock::now() -
                                                    submissionStartTime)
              .count();
      m_submitMicrosecondsAverage =
          m_submitMicrosecondsAverage == 0
              ? submitMicroseconds
              : m_submitMicrosecondsAverage * 0.95 + submitMicroseconds * 0.05;
      // Nothing recorded on `commandBuffer` itself; the texture is ready for
      // the scene graph once Graphite's work executes ahead of it in queue
      // order. The command buffer is still needed for the fallback path below.
      return;
    }
  }
#endif

  // CPU fallback: raster into a pixel buffer, then upload into the texture.
  m_rasterPixels.resize(static_cast<size_t>(pixelSize.width()) *
                        static_cast<size_t>(pixelSize.height()));
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(),
      static_cast<size_t>(pixelSize.width()) * sizeof(uint32_t));
  if (!surface)
    return;
  renderScene(surface->getCanvas(), devicePixelRatio, m_logicalSize);

  const auto submissionStartTime = Clock::now();
  QRhiResourceUpdateBatch *batch = rhi()->nextResourceUpdateBatch();
  QRhiTextureSubresourceUploadDescription sub(
      m_rasterPixels.data(), m_rasterPixels.size() * sizeof(uint32_t));
  batch->uploadTexture(texture, QRhiTextureUploadDescription({0, 0, sub}));
  commandBuffer->resourceUpdate(batch);
  const double submitMicroseconds = std::chrono::duration<double, std::micro>(
                                        Clock::now() - submissionStartTime)
                                        .count();
  m_submitMicrosecondsAverage =
      m_submitMicrosecondsAverage == 0
          ? submitMicroseconds
          : m_submitMicrosecondsAverage * 0.95 + submitMicroseconds * 0.05;
}

// ── GUI-thread side ────────────────────────────────────────────────────────

GalleryView::GalleryView(QQuickItem *parent) : QQuickRhiItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton);
  m_timer.setInterval(16);
  connect(&m_timer, &QTimer::timeout, this, [this] { update(); });
  m_timer.start();
}

GalleryView::~GalleryView() = default;

QQuickRhiItemRenderer *GalleryView::createRenderer() {
  return new GalleryViewRenderer;
}

QStringList GalleryView::sceneNames() const {
  QStringList names;
  for (const SceneDescriptor &descriptor : sceneRegistry())
    names << descriptor.name;
  return names;
}

void GalleryView::setSceneIndex(int index) {
  if (index == m_sceneIndex || index < 0 ||
      index >= static_cast<int>(sceneRegistry().size()))
    return;
  m_sceneIndex = index;
  m_sceneText.clear();
  ++m_sceneParameterRevision; // renderer re-pulls the new scene's values
  emit sceneIndexChanged();
  emit sceneTextChanged();
  emit sceneParameterValuesChanged();
  update();
}

void GalleryView::setAnimating(bool enabled) {
  if (enabled == m_animating)
    return;
  m_animating = enabled;
  if (enabled)
    m_timer.start();
  else
    m_timer.stop();
  emit animatingChanged();
  update();
}

namespace {

const SceneDescriptor *descriptorAt(int sceneIndex) {
  const auto &registry = sceneRegistry();
  if (sceneIndex < 0 || sceneIndex >= static_cast<int>(registry.size()))
    return nullptr;
  return &registry[static_cast<size_t>(sceneIndex)];
}

QVariantMap defaultParameterValues(const SceneDescriptor &descriptor) {
  QVariantMap values;
  for (const SceneParameter &parameter : descriptor.parameters)
    values.insert(parameter.id, parameter.defaultValue);
  return values;
}

} // namespace

bool GalleryView::textEditable() const {
  const SceneDescriptor *descriptor = descriptorAt(m_sceneIndex);
  return descriptor && descriptor->textEditable;
}

QVariantList GalleryView::sceneParameters() const {
  QVariantList parameters;
  const SceneDescriptor *descriptor = descriptorAt(m_sceneIndex);
  if (!descriptor)
    return parameters;
  parameters.reserve(descriptor->parameters.size());
  for (const SceneParameter &parameter : descriptor->parameters) {
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
  const SceneDescriptor *descriptor = descriptorAt(m_sceneIndex);
  return descriptor ? descriptor->controlsQml : QUrl();
}

const QVariantMap &GalleryView::parameterValuesForScene(int sceneIndex) const {
  auto values = m_sceneParameterValues.find(sceneIndex);
  if (values == m_sceneParameterValues.end()) {
    const SceneDescriptor *descriptor = descriptorAt(sceneIndex);
    values = m_sceneParameterValues.insert(
        sceneIndex,
        descriptor ? defaultParameterValues(*descriptor) : QVariantMap());
  }
  return *values;
}

QVariantMap GalleryView::sceneParameterValues() const {
  return parameterValuesForScene(m_sceneIndex);
}

void GalleryView::setSceneParameter(const QString &id, const QVariant &value) {
  const SceneDescriptor *descriptor = descriptorAt(m_sceneIndex);
  if (!descriptor)
    return;
  const auto parameter = std::find_if(
      descriptor->parameters.cbegin(), descriptor->parameters.cend(),
      [&](const SceneParameter &candidate) { return candidate.id == id; });
  if (parameter == descriptor->parameters.cend())
    return; // only declared parameters are stored

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
    clampedValue = std::clamp(value.toInt(), 0,
                              static_cast<int>(parameter->choices.size()) - 1);
    break;
  }

  QVariantMap &values = const_cast<QVariantMap &>(
      parameterValuesForScene(m_sceneIndex));
  if (values.value(id) == clampedValue)
    return;
  values.insert(id, clampedValue);
  ++m_sceneParameterRevision;
  emit sceneParameterValuesChanged();
  update();
}

void GalleryView::setSceneText(const QString &text) {
  if (text == m_sceneText)
    return;
  m_sceneText = text;
  emit sceneTextChanged();
  update();
}

void GalleryView::setFontFamily(const QString &family) {
  if (family == m_fontFamily)
    return;
  m_fontFamily = family;
  refreshFontAxes();
  emit fontFamilyChanged();
  update();
}

void GalleryView::setFontSize(qreal size) {
  const qreal clampedSize = std::clamp(size, 8.0, 200.0);
  if (clampedSize == m_fontSize)
    return;
  m_fontSize = clampedSize;
  emit fontSizeChanged();
  update();
}

QVariantList GalleryView::fontAxes() const {
  QVariantList axes;
  axes.reserve(static_cast<qsizetype>(m_fontAxes.size()));
  for (const FontAxis &axis : m_fontAxes) {
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
  for (const FontAxis &axis : m_fontAxes)
    values.insert(axis.tagName, axis.value);
  return values;
}

void GalleryView::setFontAxisValue(const QString &tag, qreal value) {
  for (FontAxis &axis : m_fontAxes) {
    if (axis.tagName != tag)
      continue;
    const float clampedValue =
        std::clamp(static_cast<float>(value), axis.minimum, axis.maximum);
    if (clampedValue == axis.value)
      return;
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
    const sk_sp<SkFontMgr> fontManager = textflow::ports::systemFontManager();
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
        for (const SkFontParameters::Variation::Axis &parameter : parameters) {
          float value = parameter.def;
          if (havePosition) {
            const auto coordinate = std::find_if(
                position.begin(), position.end(), [&](const auto &candidate) {
                  return candidate.axis == parameter.tag;
                });
            if (coordinate != position.end())
              value = coordinate->value;
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
  if (index == m_alignmentIndex)
    return;
  m_alignmentIndex = index;
  emit alignmentIndexChanged();
  update();
}

void GalleryView::setLineBreakStrategyIndex(int index) {
  index = std::clamp(index, 0, 1);
  if (index == m_lineBreakStrategyIndex)
    return;
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
  if (enabled == m_gpu)
    return;
  m_gpu = enabled;
  emit gpuChanged();
  update();
}

void GalleryView::mousePressEvent(QMouseEvent *event) {
  m_pendingClicks.push_back({static_cast<float>(event->position().x()),
                             static_cast<float>(event->position().y())});
  event->accept();
  update();
}
