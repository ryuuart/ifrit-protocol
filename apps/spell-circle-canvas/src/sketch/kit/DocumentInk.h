#pragma once

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/PaintStyle.h>

#include <utility>

namespace sigil::sketch::kit::detail {

/** A caller's explicit fill on document text. A shader changes only paint,
 *  so the document's role still controls the face, size and line layout. */
inline void documentInk(compose::Text& line, const compose::Fill& ink) {
  if (ink.ref == compose::Fill::Ref::CurrentInk) return;
  if (ink.ref == compose::Fill::Ref::Var) {
    line.ink(compose::VarRef{ink.varId});
    return;
  }
  if (ink.kind == compose::Fill::Kind::Shader) {
    weave::PaintStyle paint(SK_ColorWHITE);
    paint.foreground.setShader(ink.shaderValue);
    line.spanPaint(weave::Selector{}, std::move(paint));
  } else {
    line.ink(ink.kind == compose::Fill::Kind::Color ? ink.colorValue
                                                    : SkColors::kTransparent);
  }
}

}  // namespace sigil::sketch::kit::detail
