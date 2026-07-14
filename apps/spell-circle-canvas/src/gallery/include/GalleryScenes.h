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

/** Live control state pushed down from the QML panel. */
struct SceneParams {
  QString text;               // body text; empty → scene default
  sk_sp<SkTypeface> typeface; // null → scene default
  float fontSize = 17.0f;
  textflow::TextAlignment alignment = textflow::TextAlignment::kJustify;
  textflow::LineBreakStrategy lineBreakStrategy =
      textflow::LineBreakStrategy::kGreedy;

  // Per-layer shader/preset toggles, consulted only by scenes that report
  // supportsEffectToggles().
  bool effectGlow = true;
  bool effectOutline = true;
  bool effectShader = true;
  bool effectStars = true;
  // Glow shaping: spread dilates the glyph shape (stroke-and-fill) before
  // the blur mask is applied, keeping a solid core under a wide blur.
  // Intensity scales the glow color's alpha beyond its own hex value. Scenes
  // at small font sizes should further scale spread down; a fixed pixel
  // amount this size can otherwise merge adjacent glyphs and lines solid.
  float glowSpread = 0.6f;
  float glowIntensity = 1.3f;
};

/** Timing and output counts reported by one rendered gallery frame. */
struct FrameStats {
  double layoutMicroseconds = 0;
  int runCount = 0;
  int glyphCount = 0;
};

class Scene {
public:
  virtual ~Scene() = default;
  /** Returns the user-facing scene name. */
  virtual QString name() const = 0;
  /** Returns initial editable text, or an empty string for scene-owned text. */
  virtual QString defaultText() const { return {}; }
  /** Returns whether the gallery should expose its text editor. */
  virtual bool supportsTextEdit() const { return true; }
  /** Returns whether the gallery should expose the shader-layer toggles
   *  (glow/outline/shader/stars) in SceneParams. */
  virtual bool supportsEffectToggles() const { return false; }

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

/** Constructs every gallery scene in display order. */
std::vector<std::unique_ptr<Scene>> makeScenes();

} // namespace gallery
