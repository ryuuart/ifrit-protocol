# workaround: vcpkg's Python wrapper forces its own headers and library even
# when the build or wheel backend selected another interpreter. CMake's
# discovery keeps the development artifacts matched to that interpreter.
if(VCPKG_TARGET_TRIPLET AND COMMAND _find_package)
  _find_package(Python3 3.12 COMPONENTS Interpreter Development.Module Development.Embed REQUIRED)
else()
  find_package(Python3 3.12 COMPONENTS Interpreter Development.Module Development.Embed REQUIRED)
endif()
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

sigil_library(SigilSketchPython
  SOURCES Python.cpp Bindings.cpp Module.cpp ComposeBindings.cpp MotionBindings.cpp
          KitBindings.cpp DataBindings.cpp PenBindings.cpp ValueBindings.cpp GeometryBindings.cpp
          BrushBindings.cpp
  HEADERS Python.h
  PUBLIC SigilSketch
  PRIVATE pybind11::headers SigilSketchKit
          SigilDataDecode SigilDataQuery SigilDataScale
          SigilGeometryMeshRender SigilGeometryKit SigilMaterialPattern SigilMaterialField
          SigilImageDecode SigilImageEncode SigilDrawBrush SigilDrawBrushFormat)
target_include_directories(SigilSketchPython PRIVATE ${Python3_INCLUDE_DIRS})
# Executable consumers embed Python; an extension resolves Python's symbols
# from its importing interpreter and must not load another libpython.
target_link_libraries(SigilSketchPython INTERFACE
  "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:SIGIL_PYTHON_EXTENSION>>>:Python3::Python>")
target_compile_definitions(SigilSketchPython PRIVATE
  SIGIL_PYTHON_PACKAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  SIGIL_PYTHON_ABI="${Python3_SOABI}")

# The same registration code is a built-in module in Sketchbook and an
# extension in ordinary Python. Each process loads one native implementation.
pybind11_add_module(sigil_python NO_EXTRAS Module.cpp)
target_link_libraries(sigil_python PRIVATE SigilSketchPython)
set_target_properties(sigil_python PROPERTIES
  OUTPUT_NAME _sigil
  SIGIL_PYTHON_EXTENSION ON
  INSTALL_RPATH_USE_LINK_PATH ON
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/$<0:>")
if(APPLE)
  target_link_options(sigil_python PRIVATE "LINKER:-dead_strip_dylibs")
  # Wheel repair resolves dependencies after installation. CMake omits
  # in-tree link directories from the install rpath, so name vcpkg's path
  # explicitly; repair copies the libraries and removes this search path.
  if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set_property(TARGET sigil_python APPEND PROPERTY INSTALL_RPATH
      "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
  endif()
endif()
add_custom_target(sigil_python_package
  COMMAND ${CMAKE_COMMAND} -E rm -f
    "${CMAKE_BINARY_DIR}/python/sigil/compose.py"
    "${CMAKE_BINARY_DIR}/python/sigil/sketch.py"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/sigil" "${CMAKE_BINARY_DIR}/python/sigil")
add_dependencies(sigil_python sigil_python_package)

install(TARGETS sigil_python
  LIBRARY DESTINATION . COMPONENT SigilPython
  RUNTIME DESTINATION . COMPONENT SigilPython)
install(DIRECTORY sigil DESTINATION . COMPONENT SigilPython
  FILES_MATCHING PATTERN "*.py" PATTERN "__pycache__" EXCLUDE)
file(GLOB SIGIL_PYTHON_EXAMPLES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../sketches/python_*.py")
install(FILES ${SIGIL_PYTHON_EXAMPLES}
  DESTINATION sigil/examples COMPONENT SigilPython)
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  file(GLOB SIGIL_PYTHON_DEPENDENCY_LICENSES
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/*/copyright")
  foreach(license IN LISTS SIGIL_PYTHON_DEPENDENCY_LICENSES)
    get_filename_component(package "${license}" DIRECTORY)
    get_filename_component(package "${package}" NAME)
    install(FILES "${license}" DESTINATION "sigil/_licenses/${package}"
      COMPONENT SigilPython)
  endforeach()
endif()

sigil_test(sketch_test
  SOURCES test/PythonTest.cpp
  LIBRARIES SigilSketchPython)

add_test(NAME sketch_python_authoring
  COMMAND ${CMAKE_COMMAND} -E env
    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
    "SIGIL_TEST_SKETCHBOOK=$<TARGET_FILE:Sketchbook>"
    ${Python3_EXECUTABLE} -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}/test" -p "test_*.py")
