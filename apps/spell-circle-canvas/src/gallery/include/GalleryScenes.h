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

#include <memory>
#include <vector>

namespace gallery {

// Live control state pushed down from the QML panel.
struct SceneParams {
  QString text;               // body text; empty → scene default
  sk_sp<SkTypeface> typeface; // null → scene default
  float fontSize = 17.0f;
  textflow::Alignment alignment = textflow::Alignment::kJustify;
  textflow::Breaker breaker = textflow::Breaker::kGreedy;
};

struct FrameStats {
  double layoutUs = 0;
  int runs = 0;
  int glyphs = 0;
};

class Scene {
public:
  virtual ~Scene() = default;
  virtual QString name() const = 0;
  virtual QString defaultText() const { return {}; }
  virtual bool supportsTextEdit() const { return true; }

  // `t` is seconds since the scene became active; `frame` increments once
  // per rendered frame (frozen while paused).
  virtual FrameStats render(SkCanvas *canvas, SkISize size, double t,
                            int frame, const SceneParams &params,
                            textflow::FontContext &ctx) = 0;

  // Pointer interaction in scene coordinates (e.g. the ripple pool spawns
  // a drop where you click).
  virtual void pointerPress(SkPoint) {}
};

std::vector<std::unique_ptr<Scene>> makeScenes();

} // namespace gallery
