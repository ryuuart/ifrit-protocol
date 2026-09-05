# The qt feature: the adapters a Qt host reaches Graphite through. One
# translation unit per graphics API unwraps a QRhi's native handles and
# forwards to the graphite feature — Metal on Apple, Vulkan elsewhere —
# so exactly one API is served per build and any other QRhi backend gets
# null. Links the graphite feature (and through it SigilCoreHardware,
# whose Vulkan handle set the adapter fills) and Qt6::Core publicly,
# Qt6::GuiPrivate for the QRhi API.
find_package(Qt6 REQUIRED COMPONENTS Core GuiPrivate)
qt_standard_project_setup(REQUIRES 6.11)

if(APPLE)
  set(SIGIL_SKIA_QT_SOURCES QtInteropMetal.mm)
else()
  set(SIGIL_SKIA_QT_SOURCES QtInteropVulkan.cpp)
endif()

qt_add_library(SigilSkiaQt STATIC
  ${SIGIL_SKIA_QT_SOURCES}
  ${SIGIL_INCLUDE_DIR}/sigilskia/qt/QtInterop.h
)
sigil_qt_target(SigilSkiaQt)
target_link_libraries(SigilSkiaQt
  PUBLIC SigilSkiaGraphite Qt6::Core
  PRIVATE Qt6::GuiPrivate)
