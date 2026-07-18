#pragma once

// The TextFlow gallery's demo scenes. Each scene owns its paragraphs and
// per-frame state and renders a complete frame into an SkCanvas in logical
// (item-sized) coordinates. Scenes are deliberately built only on TextFlow's
// public API — ruby, kenten, marker highlights and the letter choreography
// are all "build on top" patterns, not library features.

#include <textflow/TextFlow.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkSize.h>

#include <QString>
#include <QVariantMap>

#include <memory>
#include <vector>

namespace gallery {

/** Live control state pushed down from the QML panel. */
struct SceneParams {
  QString text;               // body text; empty → scene default
  sk_sp<SkTypeface> typeface; // null → scene default
  float fontSize = 17.0f;
  textflow::TextAlignment alignment = textflow::TextAlignment::kJustify;
  textflow::LineBreakStrategy lineBreakStrategy =
      textflow::LineBreakStrategy::kGreedy;

  /// Scene-declared parameter values, keyed by SceneParameter::id (see
  /// SceneRegistry.h). Absent ids fall back through the typed getters, so a
  /// scene renders correctly even before the GUI ships its first values.
  QVariantMap values;

  /** Returns the bool parameter `id`, or `fallback` when absent. */
  [[nodiscard]] bool boolValue(const QString &id, bool fallback) const {
    const auto value = values.constFind(id);
    return value == values.constEnd() ? fallback : value->toBool();
  }
  /** Returns the float parameter `id`, or `fallback` when absent. */
  [[nodiscard]] float floatValue(const QString &id, float fallback) const {
    const auto value = values.constFind(id);
    return value == values.constEnd() ? fallback
                                      : static_cast<float>(value->toDouble());
  }
  /** Returns the int (or choice-index) parameter `id`, or `fallback`. */
  [[nodiscard]] int intValue(const QString &id, int fallback) const {
    const auto value = values.constFind(id);
    return value == values.constEnd() ? fallback : value->toInt();
  }
};

/** Timing and output counts reported by one rendered gallery frame. */
struct FrameStats {
  double layoutMicroseconds = 0;
  int runCount = 0;
  int glyphCount = 0;
};

/// A scene owns only its drawing and per-frame state; every piece of GUI
/// metadata (name, default text, editability, parameters, optional custom
/// controls QML) lives in the SceneDescriptor it registers via
/// REGISTER_GALLERY_SCENE (SceneRegistry.h).
class Scene {
public:
  virtual ~Scene() = default;

  /** Renders one scene frame and returns its timing and content statistics.
   *  `elapsedSeconds` starts when this scene becomes active; `frameNumber`
   *  increments once per rendered frame and freezes while paused. */
  virtual FrameStats render(SkCanvas *canvas, SkISize size,
                            double elapsedSeconds, int frameNumber,
                            const SceneParams &params,
                            textflow::FontContext &fontContext) = 0;

  /** Handles a pointer press in scene coordinates. */
  virtual void pointerPress(SkPoint position) { static_cast<void>(position); }
};

} // namespace gallery
