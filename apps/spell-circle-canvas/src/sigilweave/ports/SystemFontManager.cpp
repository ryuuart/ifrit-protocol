#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>

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

} // namespace sigil::weave::ports
