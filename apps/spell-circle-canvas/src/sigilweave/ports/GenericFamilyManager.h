#pragma once

/** @file
 * The platform font manager, answering CSS's generic family names as the
 * platform's own families.
 */

#include <include/core/SkFontMgr.h>
#include <include/core/SkRefCnt.h>

namespace sigil::weave::ports::detail {

/** @p platform, asked for a family by a generic name — `serif`,
 *  `sans-serif`, `monospace`, `system-ui` — answering with the first of
 *  the families `genericFamilies` names for it that @p platform has, and
 *  asked for anything else answering exactly as @p platform does. */
sk_sp<SkFontMgr> answeringGenericFamilies(sk_sp<SkFontMgr> platform);

}  // namespace sigil::weave::ports::detail
