#pragma once
/** @file
 * The glyph count of a finished layout, for every benchmark that reports
 * its work per glyph placed. It is the reading the tests take, so a
 * benchmark's per-glyph rate counts what a test counts.
 */

#include "../../test/support/Layouts.h"

namespace sigil::weave::bench {

using test::glyphCount;

}  // namespace sigil::weave::bench
