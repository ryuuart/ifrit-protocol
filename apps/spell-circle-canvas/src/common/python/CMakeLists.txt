sigil_library_root(SigilPython
  BRIEF "Reusable Python bindings for native Sigil library values and drawing")

sigil_library(SigilPython
  SOURCES Python.cpp Bindings.cpp ComposeBindings.cpp MotionBindings.cpp
          KitBindings.cpp DocumentBindings.cpp DataBindings.cpp IOBindings.cpp PenBindings.cpp
          ValueBindings.cpp GeometryBindings.cpp BrushBindings.cpp WeaveBindings.cpp
          WorldBindings.cpp MaterialBindings.cpp WeaveLayoutBindings.cpp PublicNames.cpp
  HEADERS Python.h Bindings.h ComposeBindings.h MotionBindings.h KitBindings.h
          DataBindings.h IOBindings.h PenBindings.h ValueBindings.h BrushBindings.h
          WeaveBindings.h GeometryBindings.h WorldBindings.h PublicNames.h
  PUBLIC pybind11::headers unofficial::skia::skia
         SigilComposeCore SigilComposeKit SigilMotionValues
  PRIVATE SigilCoreCompute SigilCoreReconcile SigilComposeBrush SigilComposeDraw
          SigilMotionBind SigilMotionClock SigilDataTable SigilDataDecode
          SigilDataQuery SigilDataScale SigilIOHub SigilIOTransport
          SigilGeometryPath SigilGeometryMesh SigilGeometryMeshCamera
          SigilGeometryMeshRender SigilGeometryKit
          SigilMaterialCore SigilMaterialSkia SigilMaterialPattern SigilMaterialField
          SigilMaterialKit SigilWorldScene SigilWorldKit
          SigilWeaveStyle SigilWeaveParagraph SigilWeaveQuery SigilWeaveLayout SigilWeavePorts SigilWeaveKit
          SigilImageDecode SigilImageEncode SigilDraw SigilDrawBrush SigilDrawBrushFormat)
target_include_directories(SigilPython SYSTEM PUBLIC ${Python3_INCLUDE_DIRS})
# Executables embed Python; extension modules resolve Python symbols from
# their importing interpreter and must not load another libpython.
target_link_libraries(SigilPython INTERFACE
  "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:SIGIL_PYTHON_EXTENSION>>>:Python3::Python>")

# This consumer deliberately links no sketch target. Common registration,
# retained callbacks and native kit values must work in another host.
sigil_test(python_test
  SOURCES test/BindingsTest.cpp
  LIBRARIES SigilPython
  DEFINITIONS SIGIL_PYTHON_PACKAGE_DIR="${SIGIL_PYTHON_PACKAGE_DIR}")

sigil_doc_probes(python
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  FLOORS 1,0,0,30,0
  LIBRARIES SigilPython)
