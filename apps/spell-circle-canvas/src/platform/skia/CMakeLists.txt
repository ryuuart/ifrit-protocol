# Generic Skia Graphite (Metal) GPU interop plumbing: standing up a
# skgpu::graphite::Context/Recorder bound to Qt's QRhi Metal device/queue,
# and wrapping an existing QRhiTexture as an SkSurface. This knows nothing
# about SpellCircle scene content — that lives in
# src/qml/SpellCircle/SkiaSceneBackend.mm, which links against this.
#
# Only added when SPELLCIRCLE_ENABLE_SKIA_CANVAS is ON (see
# src/platform/CMakeLists.txt and the top-level CMakeLists.txt).
qt_add_library(SpellCircleSkia SHARED
  SkiaGraphiteContext.mm include/SkiaGraphiteContext.h
  SkiaOffscreenSurface.mm include/SkiaOffscreenSurface.h
)

target_link_libraries(SpellCircleSkia
    PUBLIC
      Qt6::Core
      unofficial::skia::skia
    PRIVATE
      Qt6::GuiPrivate
      ${FOUNDATION_FRAMEWORK}
)

target_include_directories(SpellCircleSkia PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
