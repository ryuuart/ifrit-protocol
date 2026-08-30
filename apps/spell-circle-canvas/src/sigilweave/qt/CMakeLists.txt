# Qt ↔ Skia bridging helpers (header-only). A separate target so every
# engine feature stays Qt-free; only Qt consumers link this.
add_library(SigilWeaveQt INTERFACE
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/qt/SigilWeaveQt.h
)
target_compile_features(SigilWeaveQt INTERFACE cxx_std_20)
target_include_directories(SigilWeaveQt INTERFACE
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveQt
  INTERFACE
    Qt6::Gui
    unofficial::skia::skia
    SigilWeaveParagraph
    SigilWeaveQuery
)
