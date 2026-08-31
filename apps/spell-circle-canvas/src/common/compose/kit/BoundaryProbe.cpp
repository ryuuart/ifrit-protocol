/** @file
 * The tier boundary's NEGATIVE CONTROL. Built on purpose, expected to FAIL.
 *
 * The kit may only see compose's public headers. This TU reaches for a
 * kernel internal the way a kit header would if the boundary were not
 * structural; because SigilComposeKit's include path is `include/` alone
 * and ComposeInternal.h lives in core/, it cannot be found.
 *
 * The `compose_kit_boundary_probe` test builds this target and requires
 * "'ComposeInternal.h' file not found" in the output.
 */

#include "ComposeInternal.h"

int main() { return sizeof(sigil::compose::detail::ElementNode); }
