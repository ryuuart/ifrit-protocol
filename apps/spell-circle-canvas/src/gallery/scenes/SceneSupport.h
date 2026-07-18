#pragma once

// Shared helpers used by every gallery scene (src/gallery/scenes/*.cpp).
// The reusable machinery — rebuild/layout guards, glyph buckets, label and
// filler helpers, timing — lives in TextFlowKit (src/text/kit); this header
// keeps only the gallery's specializations of it: the palette, the
// SceneParams-aware body cache, the palette-colored caption, and a morphing
// ring path shared by more than one scene. Scene-specific state lives in
// each scene's own file.

#include "../include/GalleryScenes.h"

#include <textflowkit/TextFlowKit.h>

#include <textflow/TextFlow.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>

#include <QString>

#include <chrono>
#include <string_view>

namespace gallery {

// Scenes qualify kit types as kit:: so reading a scene makes the boundary
// between the library pattern and the scene's own choreography visible.
namespace kit = textflowkit;

using Clock = std::chrono::steady_clock;
using kit::toMicroseconds;

inline constexpr SkColor kInk = kit::palette::kInk;
inline constexpr SkColor kAccent = kit::palette::kAccent;
inline constexpr SkColor kBlue = kit::palette::kBlue;
inline constexpr SkColor kShape = kit::palette::kShape;
inline constexpr SkColor kPaper = kit::palette::kPaper;

using kit::makeStyle;

/// Caches a scene body paragraph until one of its shaping inputs changes:
/// the gallery's SceneParams front-end to a kit::RebuildGuard, resolving
/// empty panel values to the scene's defaults before they enter the key.
struct BodyCache {
  textflow::Paragraph paragraph;

  /// Rebuilds the paragraph when its text, typeface, or size has changed.
  /// Returns true when rebuilding occurred.
  bool ensure(const SceneParams &params, const QString &fallbackText,
              const sk_sp<SkTypeface> &fallbackTypeface);

private:
  kit::RebuildGuard<QString, const SkTypeface *, float> m_guard;
};

/// Resolves the preferred body serif, falling back to the context default.
sk_sp<SkTypeface> defaultSerif(textflow::FontContext &fontContext);

/// Draws a small single-line explanatory caption in the gallery's blue.
void drawCaption(SkCanvas *canvas, textflow::FontContext &fontContext,
                 std::u8string_view text, SkPoint baselineOrigin,
                 float width = 520);

/// UTF-16 captions (QString via textflowqt::toU16) cross in zero-copy.
void drawCaption(SkCanvas *canvas, textflow::FontContext &fontContext,
                 std::u16string_view text, SkPoint baselineOrigin,
                 float width = 520);

// A spiky ring whose points breathe in and out independently, around an
// even-odd hole that stays open to text. Rebuilt every frame: each frame's
// path carries a fresh generation ID, so unlike the drifting donut (cached
// flattening + pathOffset) this shape exercises live re-flattening and the
// layout adapts to the changing silhouette as it morphs.
SkPath spikyRingPath(float elapsedSeconds, float radius);

} // namespace gallery
