#pragma once

/** @file
 * @ingroup weave-ports
 *
 * Platform system-font-manager factory — the one place SigilWeave's tools,
 * tests, and consumers obtain an SkFontMgr wired to the host operating
 * system — and `pickTypeface`, the fallback chain resolved against it. Every
 * platform port hides behind the same call, so adding DirectWrite
 * (Windows) or Fontconfig (Linux) later touches only
 * SystemFontManager.cpp, never a call site.
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

/**
 * Returns the process-wide system font manager (CoreText on macOS).
 *
 * Construction enumerates the installed font set, which is far too slow to
 * repeat per call site, so one shared instance is created lazily and reused
 * for the life of the process. The returned manager is immutable and safe
 * to hand to any number of FontContexts on any thread.
 */
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

/** The first of @p families the system font manager resolves, at @p style.
 *
 *  The chain is the point: reconstructing a reference names a face that may
 *  not be installed on the machine running the code, so callers pass the
 *  face they want followed by the stand-ins they will accept. A family the
 *  chain leaves empty is passed over, so a chain assembled at run time may
 *  carry a blank where it found no name.
 *
 *  The last resort is the default family AT THE REQUESTED STYLE, not at
 *  `SkFontStyle::Normal()` — falling back to Normal would silently drop the
 *  weight the caller asked for.
 *
 *  This is the form a COMPUTED chain takes — one read out of a document, a
 *  settings file or a caller's own list, whose length is not known where
 *  the call is written. The overload below is the same call with the chain
 *  spelled out.
 *
 *  `matchFamilyStyle` walks the system font list, so a chain asked for
 *  more than once goes through `face()` below, which keeps the answer. */
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

/** THE SAME RESOLUTION, HELD. `pickTypeface` walks the installed font
 *  list on every call, so the answer is kept once per (families, style)
 *  for the life of the process and handed back on every later ask.
 *
 *      const sk_sp<SkTypeface> face =
 *          weave::ports::face({"SF Mono", "Menlo", "monospace"});
 *
 *  WHY THIS IS A CALL AND NOT A `static` AT THE CALL SITE. A local
 *  `static` holds one answer per site, so the same four families asked
 *  for in twenty places walk the list twenty times and hand back twenty
 *  faces — and a face is compared by POINTER wherever a style, a memo
 *  key or an inherited value is compared, so two resolutions of one
 *  family never compare equal and everything keyed on them re-does its
 *  work. One holder gives one answer.
 *
 *  Safe from any thread: a describe runs on whichever thread the host
 *  calls on, and the holder is guarded. The face itself is immutable and
 *  shared, exactly as `systemFontManager()`'s is.
 *
 *  This is the form a COMPUTED chain takes; the overload below is the same
 *  call with the chain spelled out. Both reach the one holder, so a chain
 *  assembled at run time and the same chain written as literals are one
 *  entry and one face. */
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
