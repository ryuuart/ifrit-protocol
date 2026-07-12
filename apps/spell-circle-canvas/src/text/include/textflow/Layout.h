#pragma once

#include "Flow.h"
#include "Paragraph.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkTextBlob.h>

#include <cstdint>
#include <vector>

namespace textflow {

class FontContext;

enum class Alignment : uint8_t { kStart, kCenter, kEnd, kJustify };
enum class Breaker : uint8_t { kGreedy, kKnuthPlass };

struct LayoutOptions {
  Alignment alignment = Alignment::kStart;
  Breaker breaker = Breaker::kGreedy;

  // Absolute line height / baseline offset; 0 → derived from the
  // paragraph's strut (first span's font metrics, single-spaced).
  float lineHeight = 0;
  float ascent = 0;

  // When `alignment` is kJustify, paragraph-final (and hard-break-final)
  // lines use this instead — InDesign's "justify with last line aligned
  // left/center/right". Set justifyLastLine for full justification.
  Alignment lastLineAlignment = Alignment::kStart;
  bool justifyLastLine = false;

  // Honor soft hyphens (U+00AD) as discretionary break points: invisible
  // unless the line breaks there, in which case a hyphen is rendered.
  // hyphenPenalty discourages the Knuth-Plass breaker from overusing them.
  bool hyphenate = true;
  float hyphenPenalty = 50.0f;

  // When justifying, also expand the zero-width gaps between ideographic
  // words (CJK has no spaces to stretch). Expansion is capped at
  // `maxIdeographicExpansion * fontSize` per gap.
  bool ideographicJustify = true;
  float maxIdeographicExpansion = 0.5f;

  // Knuth-Plass glue elasticity as fractions of the space width, and the
  // badness tolerance before the breaker falls back to a forced fit.
  float glueStretch = 0.5f;
  float glueShrink = 0.333f;
  float kpTolerance = 4000.0f;
  // Knuth-Plass must place at least one word on every interval it is given,
  // so slivers (e.g. between exclusion shapes) can be filtered out entirely.
  float kpMinInterval = 0.0f;

  // Rendering hygiene for rotated/on-path glyphs: tangents snap to this
  // many discrete directions (512 ≈ 0.7° — invisible) so animated curved
  // text doesn't mint a fresh glyph-atlas strike per glyph per frame.
  // 0 disables snapping (exact rotations for static artwork).
  int rotationSteps = 512;
};

// One draw call: a shared word blob translated to `origin`, or a fully
// positioned RSXform blob (contour/rotated intervals) drawn at (0,0).
// Placeholder runs carry no blob at all — just the flow position where the
// caller should draw its inline object (see Layout::placeholderRects).
struct PlacedRun {
  sk_sp<SkTextBlob> blob;
  ShapedWordRef shaped;    // glyph source (batched drawing, choreography)
  SkPoint origin = {0, 0};
  uint32_t styleIndex = 0; // paint lookup into Paragraph::spans()
  uint32_t wordIndex = 0;  // which Word produced this run
  int line = 0;
  bool transformed = false; // RSXform blob (positions baked into the blob)
  int placeholder = -1;     // ≥0: index into Paragraph::placeholders()
};

struct Layout {
  std::vector<PlacedRun> runs;
  int lineCount = 0;
  // First word that found no room (geometry exhausted); ~0u when all fit.
  uint32_t firstUnplacedWord = ~0u;

  bool overflowed() const { return firstUnplacedWord != ~0u; }

  // Draws every run, resolving each run's paint from the paragraph's
  // current spans — so paint-only tweaks show up without any relayout.
  // `overridePaint` replaces every span's paint (labels drawn in a caller-
  // chosen color without touching the paragraph).
  void draw(SkCanvas *canvas, const Paragraph &paragraph,
            const PaintStyle *overridePaint = nullptr) const;

  // Same output, minimal draw calls: horizontal runs are merged into one
  // SkCanvas::drawGlyphs per (font, paint) bucket instead of one
  // drawTextBlob per word — the difference between hundreds of GPU text
  // draws per frame and a handful. Transformed runs fall back to their
  // baked blobs.
  void drawBatched(SkCanvas *canvas, const Paragraph &paragraph,
                   const PaintStyle *overridePaint = nullptr) const;

  // Where every inline placeholder landed, ready to draw pills/images into.
  struct PlacedPlaceholder {
    int index = 0; // into Paragraph::placeholders()
    SkRect rect = SkRect::MakeEmpty();
    int line = 0;
  };
  std::vector<PlacedPlaceholder>
  placeholderRects(const Paragraph &paragraph) const;
};

// Lays `paragraph` out into `geometry`. Ensures the paragraph is shaped
// (cache-hot when little changed), breaks it into lines with the configured
// breaker, and returns positioned runs backed by shared word blobs.
Layout layoutParagraph(FontContext &ctx, Paragraph &paragraph,
                       FlowGeometry &geometry,
                       const LayoutOptions &options = {});

} // namespace textflow
