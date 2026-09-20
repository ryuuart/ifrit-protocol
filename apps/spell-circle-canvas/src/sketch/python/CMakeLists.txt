# The sketch adapter adds session ownership and live-source loading to the
# reusable native bindings. Module.cpp assembles the one built-in module;
# the Python package also compiles it as its extension entry point.
# host/ is what a sketch is handed, kit/ the specimen values it dresses
# with; one file per package, all named here.
sigil_library(SigilSketchPython
  SOURCES
    Module.cpp host/Context.cpp host/DeviceRuntimes.cpp host/Entries.cpp
    host/Runtime.cpp host/Schema.cpp host/SetSketches.cpp host/WorldSet.cpp
    kit/Charts.cpp kit/Content.cpp kit/Legends.cpp kit/Panels.cpp
    kit/Rows.cpp kit/Specimen.cpp kit/Streams.cpp
  HEADERS Python.h
  PUBLIC SigilSketch
  PRIVATE SigilPython SigilSketchKit SigilWorldFrame
          SigilGeometryMeshPopDevice SigilGeometryMeshRenderDevice
          SigilGeometryDevice SigilDataDecode)
if(TARGET SigilScry)
  target_link_libraries(SigilSketchPython PRIVATE SigilScry)
  target_compile_definitions(SigilSketchPython PRIVATE SIGIL_PYTHON_HAS_SCRY)
endif()
target_compile_definitions(SigilSketchPython PRIVATE
  SIGIL_PYTHON_PACKAGE_DIR="${SIGIL_PYTHON_PACKAGE_DIR}"
  SIGIL_PYTHON_ABI="${Python3_SOABI}")

sigil_test(sketch_test
  SOURCES test/PythonTest.cpp
  LIBRARIES SigilSketchPython SigilPython)

add_test(NAME sketch_python_registration
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/test/registration.py)
