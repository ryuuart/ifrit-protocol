#pragma once

/** @file
 * The kit's own value noise, in the language a target speaks.
 *
 * The text is one file, written in Slang and crossed into SkSL by the
 * core's crossing, so a grained surface cannot mean two different noises
 * on two backends. Private to the kit: a consumer composing its own
 * grained body writes its own noise or asks for a preset.
 */

#include <sigilmaterial/core/Target.h>

#include <string>

namespace sigil::material::kit {

/** The hash, the value noise, the three-octave fold, the grain fold and
 *  the lattice fleck, as source in @p target, ready to be prepended to a
 *  body that calls them. */
const std::string& noisePrelude(Target target);

}  // namespace sigil::material::kit
