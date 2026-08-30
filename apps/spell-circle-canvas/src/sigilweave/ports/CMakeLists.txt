# Platform glue kept out of the engine: the system-font-manager factory
# (CoreText today; DirectWrite and Fontconfig slot into SystemFontManager.cpp).
# Every engine feature accepts any SkFontMgr and never links this; only
# tools and apps that want the OS fonts do.
add_library(SigilWeavePorts STATIC
  SystemFontManager.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/ports/SystemFontManager.h
)
target_compile_features(SigilWeavePorts PUBLIC cxx_std_20)
target_include_directories(SigilWeavePorts PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeavePorts
  PUBLIC
    unofficial::skia::skia
)
