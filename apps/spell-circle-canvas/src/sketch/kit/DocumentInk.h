#pragma once

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilweave/query/Selector.h>

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
    line.span(weave::Selector{}, compose::Declarations().ink(ink));
  } else {
    line.ink(ink.kind == compose::Fill::Kind::Color ? ink.colorValue
                                                    : SkColors::kTransparent);
  }
}

}  // namespace sigil::sketch::kit::detail
