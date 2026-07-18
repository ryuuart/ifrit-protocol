# Generic Skia Graphite GPU interop plumbing: standing up a
# skgpu::graphite::Context/Recorder bound to Qt's QRhi native device/queue,
# and wrapping an existing QRhiTexture as an SkSurface. This knows nothing
# about SpellCircle scene content — that lives in
# src/qml/SpellCircle/SkiaSceneBackend.cpp, which links against this.
#
# The graphics API is a build-time choice of translation units: Metal on
# Apple platforms, Vulkan everywhere else (a Windows bring-up draft — run
# with QSG_RHI_BACKEND=vulkan there; under any other QRhi backend the
# factory returns null and the renderer falls back to QCanvasPainter).
#
# Only added when SPELLCIRCLE_ENABLE_SKIA_CANVAS is ON (see
# src/platform/CMakeLists.txt and the top-level CMakeLists.txt).
if(APPLE)
  set(SPELLCIRCLE_SKIA_BACKEND_SOURCES
      SkiaGraphiteContextMetal.mm
      SkiaOffscreenSurfaceMetal.mm)
else()
  set(SPELLCIRCLE_SKIA_BACKEND_SOURCES
      SkiaGraphiteContextVulkan.cpp
      SkiaOffscreenSurfaceVulkan.cpp)
endif()

qt_add_library(SpellCircleSkia SHARED
  SkiaGraphiteContextCommon.cpp include/SkiaGraphiteContext.h
  SkiaOffscreenSurfaceCommon.cpp include/SkiaOffscreenSurface.h
  ${SPELLCIRCLE_SKIA_BACKEND_SOURCES}
)

target_link_libraries(SpellCircleSkia
    PUBLIC
      Qt6::Core
      unofficial::skia::skia
    PRIVATE
      Qt6::GuiPrivate
)

if(APPLE)
  target_link_libraries(SpellCircleSkia PRIVATE ${FOUNDATION_FRAMEWORK})
endif()

target_include_directories(SpellCircleSkia PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
