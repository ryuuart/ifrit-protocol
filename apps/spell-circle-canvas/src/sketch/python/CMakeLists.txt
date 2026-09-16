find_package(Python3 3.12 COMPONENTS Interpreter Development.Module Development.Embed REQUIRED)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

target_sources(SigilSketch PRIVATE Python.cpp Bindings.cpp Module.cpp)
target_link_libraries(SigilSketch PRIVATE pybind11::embed)
target_compile_definitions(SigilSketch PUBLIC SIGILSKETCH_PYTHON)
target_compile_definitions(SigilSketch PRIVATE
  SIGIL_PYTHON_PACKAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# The same registration code is a built-in module in Sketchbook and an
# extension in ordinary Python. Each process loads one native implementation.
pybind11_add_module(sigil_python NO_EXTRAS Module.cpp)
target_link_libraries(sigil_python PRIVATE SigilSketch)
set_target_properties(sigil_python PROPERTIES
  OUTPUT_NAME _sigil
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/$<0:>")
add_custom_target(sigil_python_package
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/sigil" "${CMAKE_BINARY_DIR}/python/sigil")
add_dependencies(sigil_python sigil_python_package)

sigil_test(sketch_test
  SOURCES test/PythonTest.cpp
  LIBRARIES SigilSketch)

add_test(NAME sketch_python_authoring
  COMMAND ${CMAKE_COMMAND} -E env
    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
    ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test/test_authoring.py")
