// The tier boundary's NEGATIVE CONTROL. Built on purpose, expected to FAIL.
//
// The kit may only see compose's public headers. This TU reaches for a
// kernel internal the way a kit header would if the boundary were not
// structural; because SigilComposeKit's include path is `include/` alone
// and ComposeInternal.h lives in the source root, it cannot be found.
//
//   cmake --build build --config Debug --target compose_kit_boundary_probe
//
// must fail with "'ComposeInternal.h' file not found". It is
// EXCLUDE_FROM_ALL and not registered as a ctest — a test that must fail to
// build cannot live in a gtest binary, and registering it would make the
// dashboard report a build error as a pass.

#include "ComposeInternal.h"

int main() { return sizeof(sigil::compose::detail::ElementNode); }
