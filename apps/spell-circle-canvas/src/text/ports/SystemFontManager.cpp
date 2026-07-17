#include <textflow/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>

#if defined(__APPLE__)
#include <include/ports/SkFontMgr_mac_ct.h>
#endif

namespace textflow::ports {

sk_sp<SkFontMgr> systemFontManager() {
#if defined(__APPLE__)
  static const sk_sp<SkFontMgr> manager = SkFontMgr_New_CoreText(nullptr);
#else
  // Ports for other platforms slot in here: SkFontMgr_New_DirectWrite() on
  // Windows, SkFontMgr_New_FontConfig() on Linux. Until one lands, an empty
  // manager keeps the build honest instead of hiding the gap behind #error.
  static const sk_sp<SkFontMgr> manager = SkFontMgr::RefEmpty();
#endif
  return manager;
}

} // namespace textflow::ports
