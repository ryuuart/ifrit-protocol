# The sketch adapter adds session ownership and live-source loading to the
# reusable native bindings. Module.cpp assembles the one built-in module;
# the Python package also compiles it as its extension entry point.
sigil_library(SigilSketchPython
  SOURCES Python.cpp KitBindings.cpp Module.cpp
  HEADERS Python.h
  PUBLIC SigilSketch
  PRIVATE SigilPython SigilSketchKit)
target_compile_definitions(SigilSketchPython PRIVATE
  SIGIL_PYTHON_PACKAGE_DIR="${SIGIL_PYTHON_PACKAGE_DIR}"
  SIGIL_PYTHON_ABI="${Python3_SOABI}")

sigil_test(sketch_test
  SOURCES test/PythonTest.cpp
  LIBRARIES SigilSketchPython SigilPython)

add_test(NAME sketch_python_registration
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/test/registration.py)
