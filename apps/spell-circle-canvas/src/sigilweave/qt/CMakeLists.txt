# Qt ↔ Skia bridging helpers (header-only). A separate target so every
# engine feature stays Qt-free; only Qt consumers link this.
sigil_library(SigilWeaveQt
  HEADERS SigilWeaveQt.h
  INTERFACE Qt6::Gui unofficial::skia::skia SigilWeaveParagraph
            SigilWeaveQuery)
