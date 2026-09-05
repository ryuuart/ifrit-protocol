#pragma once
// Support for compose_text_test: text data, the text pass, vertical
// writing, motion along paths and the text-fx presets. The binary links
// the typography headers and the shape catalog — motion along a path and
// an upright column are exercised on `geometry::shapes::` silhouettes — and
// nothing that strokes or fills.

#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Legibility.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include "ShapeTestSupport.h"

namespace {

/// White type in the instrument that keeps a combining mark as its own
/// glyph. A face that composes a base and its mark into one glyph leaves a
/// case about clusters with no multi-glyph cluster to be about, and which
/// faces do that is the machine's business rather than the library's.
sigil::weave::TextStyle markStyle(float size) {
  sigil::weave::TextStyle style = whiteStyle(size);
  style.shaping.typeface = sigil::test::instrument::marks();
  return style;
}

}  // namespace
