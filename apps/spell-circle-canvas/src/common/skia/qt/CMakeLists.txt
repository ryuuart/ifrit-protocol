# The qt feature: the adapters a Qt host reaches Graphite through. One
# translation unit per graphics API unwraps a QRhi's native handles and
# forwards to the graphite feature — Metal on Apple, Vulkan elsewhere —
# so exactly one API is served per build and any other QRhi backend gets
# null. Links the graphite feature and Qt6::Core publicly, Qt6::GuiPrivate
# for the QRhi API.
if(APPLE)
  set(SIGIL_SKIA_QT_SOURCES QtInteropMetal.mm)
else()
  set(SIGIL_SKIA_QT_SOURCES QtInteropVulkan.cpp)
endif()

qt_add_library(SigilSkiaQt STATIC
  ${SIGIL_SKIA_QT_SOURCES}
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/qt/QtInterop.h
)
target_compile_features(SigilSkiaQt PUBLIC cxx_std_20)
target_link_libraries(SigilSkiaQt
  PUBLIC SigilSkiaGraphite Qt6::Core
  PRIVATE Qt6::GuiPrivate)
