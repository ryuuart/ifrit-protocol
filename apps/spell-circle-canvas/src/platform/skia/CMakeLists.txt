# Generic Skia Graphite GPU interop plumbing: standing up a
# skgpu::graphite::Context/Recorder on a native device/queue, and wrapping
# an existing native texture as an SkSurface. This knows nothing about
# SpellCircle scene content — that lives in src/scene/SceneRenderer.cpp and
# src/qml/SpellCircle/SkiaSceneBackend.cpp, which link against this.
#
# Split into two targets so the native macOS app can use Graphite without
# pulling in Qt:
#  - SpellCircleSkia: Qt-free. On Apple it carries the Metal bring-up
#    (createMetal + the void* texture wrap); elsewhere it is just the
#    API-independent common code.
#  - SpellCircleSkiaQt: the QRhi adapters. On Apple these unwrap native
#    Metal handles and forward to SpellCircleSkia; elsewhere they are the
#    Vulkan bring-up TUs themselves (a Windows draft — run with
#    QSG_RHI_BACKEND=vulkan there; under any other QRhi backend the factory
#    returns null and the renderer falls back to QCanvasPainter).
#
# Only added when SPELLCIRCLE_ENABLE_SKIA_CANVAS is ON (see
# src/platform/CMakeLists.txt and the top-level CMakeLists.txt).
if(APPLE)
  set(SPELLCIRCLE_SKIA_NATIVE_SOURCES
      SkiaGraphiteContextMetal.mm
      SkiaOffscreenSurfaceMetal.mm)
  set(SPELLCIRCLE_SKIA_QT_SOURCES
      SkiaQtInteropMetal.mm)
else()
  set(SPELLCIRCLE_SKIA_NATIVE_SOURCES)
  set(SPELLCIRCLE_SKIA_QT_SOURCES
      SkiaGraphiteContextVulkan.cpp
      SkiaOffscreenSurfaceVulkan.cpp)
endif()

add_library(SpellCircleSkia STATIC
  SkiaGraphiteContextCommon.cpp include/SkiaGraphiteContext.h
  SkiaOffscreenSurfaceCommon.cpp include/SkiaOffscreenSurface.h
  ${SPELLCIRCLE_SKIA_NATIVE_SOURCES}
)

target_link_libraries(SpellCircleSkia
    PUBLIC
      unofficial::skia::skia
)

if(APPLE)
  target_link_libraries(SpellCircleSkia PRIVATE ${FOUNDATION_FRAMEWORK})
endif()

target_include_directories(SpellCircleSkia PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

qt_add_library(SpellCircleSkiaQt STATIC
  ${SPELLCIRCLE_SKIA_QT_SOURCES}
)

target_link_libraries(SpellCircleSkiaQt
    PUBLIC
      SpellCircleSkia
      Qt6::Core
    PRIVATE
      Qt6::GuiPrivate
)
