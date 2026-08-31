#pragma once

/** @file
 * The kind seam: what a sketch draws through, held as a value.
 */

#include <sigilcore/comparable/Erased.h>

#include <memory>
#include <string_view>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

class Assets;
class Session;

/** THE OPERATIONS A KIND ANSWERS: open one running instance.
 *
 *  A kind knows exactly one runtime and exactly one sketch body — the
 *  two are inseparable, because the body is written against the
 *  runtime's own description language — so opening yields a fresh body
 *  and the state its runtime keeps for it, together.
 *
 *  @p fonts is the text the body measures and shapes with; @p assets is
 *  what it reaches for that it did not generate. Both outlive every
 *  session opened against them. */
struct KindOps {
  virtual ~KindOps() = default;
  /** What this kind draws through, named — how a host selects part of a
   *  registry it does not otherwise interpret. */
  [[nodiscard]] virtual std::string_view runtime() const = 0;
  [[nodiscard]] virtual std::unique_ptr<Session> open(weave::FontContext& fonts,
                                                      Assets& assets) const = 0;
};

/** WHAT A SKETCH DRAWS, as a value.
 *
 *  A drawn tree and a lit set are two runtimes, not two namespaces: a
 *  host holds the kind, asks it for a session and drives that. So a
 *  third runtime is a value someone constructs and hands to
 *  SIGIL_SKETCH, never a case someone adds to a switch in a host — and
 *  the host that lists, sweeps and photographs sketches does not change
 *  when one arrives. */
using Kind = core::Erased<KindOps>;

}  // namespace sigil::sketch
