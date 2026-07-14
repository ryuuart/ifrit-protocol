#pragma once

// Shared helpers used by every gallery scene (src/gallery/scenes/*.cpp):
// the shared palette, a body-paragraph cache driven by the panel's live
// controls, and a couple of small generators (mixed-script filler text, a
// morphing spiky ring path) reused by more than one scene. Nothing here is
// scene-specific; scene-specific state lives in each scene's own file.

#include "../include/GalleryScenes.h"

#include <textflow/TextFlow.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>

#include <QString>

#include <chrono>

namespace gallery {

using Clock = std::chrono::steady_clock;

inline constexpr SkColor kInk = 0xFF23252B;
inline constexpr SkColor kAccent = 0xFFC63D2F;
inline constexpr SkColor kBlue = 0xFF2B5AA7;
inline constexpr SkColor kShape = 0x33808A99;
inline constexpr SkColor kPaper = 0xFFFAF7F0;

/// Converts a steady-clock duration to the unit used by the gallery HUD.
double toMicroseconds(Clock::duration duration);

/// Creates the single-span styles shared by the gallery scenes.
textflow::TextStyle makeStyle(float fontSize, SkColor color,
                              const char *language = "",
                              sk_sp<SkTypeface> typeface = nullptr);

/// Caches a scene body paragraph until one of its shaping inputs changes.
struct BodyCache {
  textflow::Paragraph paragraph;
  QString builtText;
  const SkTypeface *builtTypeface = nullptr;
  float builtFontSize = 0;

  /// Rebuilds the paragraph when its text, typeface, or size has changed.
  /// Returns true when rebuilding occurred.
  bool ensure(const SceneParams &params, const QString &fallbackText,
              const sk_sp<SkTypeface> &fallbackTypeface);
};

/// Resolves the preferred body serif, falling back to the context default.
sk_sp<SkTypeface> defaultSerif(textflow::FontContext &fontContext);

/// Draws a small single-line explanatory caption.
void drawCaption(SkCanvas *canvas, textflow::FontContext &fontContext,
                 const char *text, SkPoint baselineOrigin, float width = 520);

/// Builds mixed Latin/CJK filler in alternating color chunks.
textflow::Paragraph makeBigParagraph(int wordCount, float fontSize);

// A spiky ring whose points breathe in and out independently, around an
// even-odd hole that stays open to text. Rebuilt every frame: each frame's
// path carries a fresh generation ID, so unlike the drifting donut (cached
// flattening + pathOffset) this shape exercises live re-flattening and the
// layout adapts to the changing silhouette as it morphs.
SkPath spikyRingPath(float elapsedSeconds, float radius);

} // namespace gallery
