#pragma once

/** @file
 * The link-boundary positive control every SigilMotion test opens with.
 */

// POSITIVE CONTROL for the "SigilMotion alone" tests. They claim a
// consumer can drive these values without linking a drawing library, and
// the claim would pass for the wrong reason if a drawing library happened
// to be on the include path anyway. A motion test target links one motion
// feature and gtest only, so a rendering library's headers must be
// UNREACHABLE from it. If SigilMotion grows a link edge that drags them
// in, the build stops rather than quietly hollowing the tests out.
#if __has_include(<sigilcompose/Compose.h>)
#error \
    "a drawing library's headers are reachable from a SigilMotion test — \
SigilMotion stands without a retained runtime under it, and these tests \
no longer prove that."
#endif
