#pragma once

/** @file
 * SigilCompose env — SigilCore's inherited-value channel under the compose
 * name: env::Provide binds a value for a describe scope, env::inherited
 * reads it, and the detail:: snapshot types are what a memo captures so
 * that its environment is part of its key. Nothing here is defined by
 * this library.
 *
 * Passing `const Theme&` is idiomatic for your OWN components; what has
 * no answer without this is the library's own — a `feed::`, a decoration
 * nested four levels down — each of which must otherwise be handed its
 * colours by whoever composes it, so a theme change is a mechanical edit
 * at every call site. The alternative shape, resolving a theme at PAINT
 * through bound Outputs, trades the wrong way: it makes every themed node
 * permanently volatile and therefore uncacheable, paying per frame
 * forever to save work on the rare frame where the theme actually
 * changes. There is deliberately NO library-wide `Theme` type: bindings
 * are keyed by C++ TYPE, and the key a library component uses is its own
 * existing props type (`feed::TextOptions`).
 */

#include <sigilcore/reconcile/Env.h>

namespace sigil::compose {

/** `env::Provide`, `env::inherited`, `env::inheritedOr` and `env::bound`,
 *  as SigilCore defines them. */
namespace env = ::sigil::core::env;

namespace detail {
using ::sigil::core::detail::EnvEntry;
using ::sigil::core::detail::envEqual;
using ::sigil::core::detail::EnvRestore;
using ::sigil::core::detail::EnvSnapshot;
using ::sigil::core::detail::envStack;
}  // namespace detail

}  // namespace sigil::compose
