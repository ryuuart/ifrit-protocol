#pragma once

/** @file
 * @ingroup weave-ports
 *
 * Platform system-font-manager factory — the one place SigilWeave's
 * tools, tests and consumers obtain an SkFontMgr wired to the host
 * operating system — and the fallback chain resolved against it. Every
 * platform port hides behind the same call.
 */

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>

/** WHERE THE ENGINE MEETS THE OPERATING SYSTEM: the factory that hands
 *  back the platform's installed font set as a Skia font manager. It is
 *  the one place a host name appears in SigilWeave, and it is a separate
 *  target, so the engine itself binds to no operating system and a
 *  caller with its own font set never links this. */
namespace sigil::weave::ports {

/** Returns the process-wide system font manager, created lazily and
 * reused for the life of the process because construction enumerates the
 * installed font set. It is immutable and safe to hand to any number of
 * font contexts on any thread. */
sk_sp<SkFontMgr> systemFontManager();

namespace detail {
/** A chain spelled out at a call site, as the views the resolving calls
 *  read. A name written as a null pointer becomes an empty view, which
 *  those calls pass over. */
inline std::vector<std::string_view> familyChain(
    std::initializer_list<const char*> families) {
  std::vector<std::string_view> chain;
  chain.reserve(families.size());
  for (const char* family : families)
    chain.emplace_back(family != nullptr ? family : "");
  return chain;
}
}  // namespace detail

/** The first of @p families the system font manager resolves, at
 *  @p style: the face wanted, then the stand-ins accepted, an empty name
 *  being passed over. The last resort is the default family AT THE
 *  REQUESTED STYLE. This is the form a COMPUTED chain takes.
 *  @trap It walks the system font list on every call, so a chain asked
 *  for more than once goes through `face` instead. */
sk_sp<SkTypeface> pickTypeface(std::span<const std::string_view> families,
                               SkFontStyle style = SkFontStyle::Normal());

/** `pickTypeface` over a chain spelled out where the call is written. */
inline sk_sp<SkTypeface> pickTypeface(
    std::initializer_list<const char*> families,
    SkFontStyle style = SkFontStyle::Normal()) {
  const std::vector<std::string_view> chain = detail::familyChain(families);
  return pickTypeface(std::span<const std::string_view>(chain), style);
}

/** `pickTypeface` spelled with a weight and a slant, for the (common) case
 *  where the caller has those two numbers and not an SkFontStyle. */
inline sk_sp<SkTypeface> pickTypeface(
    std::initializer_list<const char*> families, int weight,
    SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant) {
  return pickTypeface(families,
                      SkFontStyle(weight, SkFontStyle::kNormal_Width, slant));
}

/** THE SAME RESOLUTION, HELD once per families-and-style pair for the
 *  life of the process, and safe from any thread. One holder gives one
 *  answer, a computed chain and the same chain in literals being one
 *  entry, and a face is compared by POINTER wherever a style is.
 *  @trap A `static` at the call site holds one answer per SITE, so the
 *  same families resolved in twenty places never compare equal. */
sk_sp<SkTypeface> face(std::span<const std::string_view> families,
                       SkFontStyle style = SkFontStyle::Normal());

/** `face` over a chain spelled out where the call is written. */
inline sk_sp<SkTypeface> face(std::initializer_list<const char*> families,
                              SkFontStyle style = SkFontStyle::Normal()) {
  const std::vector<std::string_view> chain = detail::familyChain(families);
  return face(std::span<const std::string_view>(chain), style);
}

/** `face` spelled with a weight and a slant, matching the `pickTypeface`
 *  overload above. */
inline sk_sp<SkTypeface> face(
    std::initializer_list<const char*> families, int weight,
    SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant) {
  return face(families, SkFontStyle(weight, SkFontStyle::kNormal_Width, slant));
}

}  // namespace sigil::weave::ports
