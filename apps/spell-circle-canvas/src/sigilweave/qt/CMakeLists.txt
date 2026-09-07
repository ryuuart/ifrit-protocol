# Qt ↔ Skia bridging helpers (header-only). A separate target so every
# engine feature stays Qt-free; only Qt consumers link this.
find_package(Qt6 REQUIRED COMPONENTS Gui)
qt_standard_project_setup(REQUIRES 6.11)
find_package(unofficial-skia CONFIG REQUIRED)

sigil_library(SigilWeaveQt
  HEADERS SigilWeaveQt.h
  INTERFACE Qt6::Gui unofficial::skia::skia SigilWeaveParagraph
            SigilWeaveQuery)
