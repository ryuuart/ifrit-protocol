# Platform glue kept out of the engine: the system-font-manager factory
# (CoreText on Apple; DirectWrite and Fontconfig slot into SystemFontManager.cpp).
# Every engine feature accepts any SkFontMgr and never links this; only
# tools and apps that want the OS fonts do.
sigil_library(SigilWeavePorts
  SOURCES SystemFontManager.cpp
  HEADERS SystemFontManager.h
  PUBLIC unofficial::skia::skia
  PRIVATE Boost::unordered)
