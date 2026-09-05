/** @file
 * The platform font manager: CoreText on macOS, DirectWrite on Windows,
 * constructed once per process because enumerating the installed fonts is
 * what it costs.
 */

#include <include/core/SkFontMgr.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <boost/unordered/unordered_flat_map.hpp>
#include <mutex>
#include <string>

#if defined(__APPLE__)
#include <include/ports/SkFontMgr_mac_ct.h>
#elif defined(_WIN32)
#include <include/ports/SkTypeface_win.h>
#endif

namespace sigil::weave::ports {

sk_sp<SkFontMgr> systemFontManager() {
#if defined(__APPLE__)
  static const sk_sp<SkFontMgr> manager = SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
  // DirectWrite over the default factory and system font collection.
  // Windows bring-up draft: compiles out on macOS, untested until the
  // Windows port lands.
  static const sk_sp<SkFontMgr> manager = SkFontMgr_New_DirectWrite();
#else
  // Ports for other platforms slot in here: SkFontMgr_New_FontConfig() on
  // Linux. Until one lands, an empty manager keeps the build honest instead
  // of hiding the gap behind #error.
  static const sk_sp<SkFontMgr> manager = SkFontMgr::RefEmpty();
#endif
  return manager;
}

sk_sp<SkTypeface> face(std::initializer_list<const char*> families,
                       SkFontStyle style) {
  // The families in order, then the three numbers a style is: two asks
  // that differ anywhere differ here, and two that agree share an entry.
  // A separator no family name carries keeps {"A","BC"} apart from
  // {"AB","C"}.
  std::string key;
  for (const char* family : families) {
    if (family != nullptr) key.append(family);
    key.push_back('\n');
  }
  key.append(std::to_string(style.weight()))
      .append("/")
      .append(std::to_string(style.width()))
      .append("/")
      .append(std::to_string((int)style.slant()));

  static std::mutex guard;
  static boost::unordered_flat_map<std::string, sk_sp<SkTypeface>> resolved;
  const std::lock_guard<std::mutex> held(guard);
  auto found = resolved.find(key);
  if (found != resolved.end()) return found->second;
  // The walk happens under the lock, so a family two threads ask for at
  // once is walked once. It is a startup cost paid per distinct ask, and
  // holding the lock across it is what makes the answer single.
  return resolved.emplace(std::move(key), pickTypeface(families, style))
      .first->second;
}

}  // namespace sigil::weave::ports
