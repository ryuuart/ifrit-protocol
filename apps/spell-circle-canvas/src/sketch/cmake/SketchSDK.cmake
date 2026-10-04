# Native metadata belongs to the runtime's library usage requirements.
include("${CMAKE_CURRENT_LIST_DIR}/SketchPlugin.cmake")

function(sigil_sketch_sdk)
  set(directory "${CMAKE_BINARY_DIR}/sdk/$<CONFIG>")
  set_property(TARGET SigilSketch PROPERTY SIGIL_SKETCH_SDK_DIRECTORY "${directory}")
  set_property(GLOBAL APPEND PROPERTY SIGIL_SKETCH_PLUGIN_ORIGINS SigilSketch)
  add_library(sigil_sketch_compile_surface OBJECT EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NativeSurface.cpp")
  target_link_libraries(sigil_sketch_compile_surface PRIVATE "$<COMPILE_ONLY:SigilSketch>")
  set_target_properties(sigil_sketch_compile_surface PROPERTIES POSITION_INDEPENDENT_CODE ON)
  add_custom_target(SigilSketchSDK)
  add_custom_target(sketch_flags)
  target_include_directories(SigilSketch PRIVATE "${directory}")
  add_dependencies(SigilSketch SigilSketchSDK)
  # A consumer may declare additional origins after this library is added.
  cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}" CALL _sigil_sketch_sdk_generate)
endfunction()

# Every library the tree publishes, by walking its directories. Test support
# is not an origin: a library whose name says it is testing support, and
# every target declared in a `test` directory, stays out of the identity,
# the boundaries and the package. A fixture that must be an origin queues
# itself on SIGIL_SKETCH_PLUGIN_ORIGINS.
function(_sigil_sketch_collect_origins directory out)
  set(origins)
  file(RELATIVE_PATH relative "${PROJECT_SOURCE_DIR}" "${directory}")
  if("/${relative}/" MATCHES "/test/")
    set(${out} "" PARENT_SCOPE)
    return()
  endif()
  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS targets)
    if(target MATCHES "Testing")
      continue()
    endif()
    get_target_property(type "${target}" TYPE)
    if(target MATCHES "^Sigil" AND (type STREQUAL STATIC_LIBRARY OR type STREQUAL INTERFACE_LIBRARY))
      list(APPEND origins "${target}")
    endif()
  endforeach()
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    _sigil_sketch_collect_origins("${child}" child_origins)
    list(APPEND origins ${child_origins})
  endforeach()
  set(${out} "${origins}" PARENT_SCOPE)
endfunction()

function(_sigil_sketch_sdk_generate)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  get_property(queue GLOBAL PROPERTY SIGIL_SKETCH_PLUGIN_ORIGINS)
  _sigil_sketch_collect_origins("${PROJECT_SOURCE_DIR}" published)
  list(APPEND queue ${published})
  set(origins)
  set(aliases)
  while(queue)
    list(POP_FRONT queue origin)
    if(origin IN_LIST origins)
      continue()
    endif()
    if(NOT TARGET "${origin}")
      message(WARNING "Native sketch origin '${origin}' is not a target here; "
        "its public headers are left out of the plugin build identity")
      continue()
    endif()
    get_target_property(alias "${origin}" ALIASED_TARGET)
    if(alias)
      list(APPEND aliases "${origin}=${alias}")
      list(APPEND queue "${alias}")
      continue()
    endif()
    list(APPEND origins "${origin}")
    get_target_property(links "${origin}" INTERFACE_LINK_LIBRARIES)
    if(NOT links)
      set(links)
    endif()
    list(FILTER links EXCLUDE REGEX "^\\$<LINK_ONLY:")
    set_property(TARGET "${origin}" PROPERTY SIGIL_SKETCH_NATIVE_PUBLIC_LINKS "${links}")
    set(dependencies)
    foreach(link IN LISTS links)
      if(link MATCHES "^\\$<LINK_ONLY:")
        continue()
      endif()
      string(REGEX REPLACE "^\\$<(BUILD_INTERFACE|COMPILE_ONLY):(.+)>$" "\\2" link "${link}")
      # A name that reads as a target but resolves to none would drop its
      # headers from the identity without a word, so it is said. A name
      # behind TARGET_EXISTS or TARGET_NAME_IF_EXISTS is optional by its own
      # statement.
      set(named_optionally FALSE)
      if(TARGET "${link}")
        set(candidates "${link}")
      elseif(link MATCHES "\\$<")
        string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_:.-]*" candidates "${link}")
        if(link MATCHES "TARGET_(NAME_IF_)?EXISTS")
          set(named_optionally TRUE)
        endif()
      else()
        set(candidates "${link}")
      endif()
      foreach(candidate IN LISTS candidates)
        if(NOT TARGET "${candidate}")
          if(NOT named_optionally AND
             (candidate MATCHES "::" OR candidate MATCHES "^Sigil"))
            message(WARNING "Native sketch origin '${origin}' links '${candidate}', "
              "which is not a target here; its public headers are left out of "
              "the plugin build identity")
          endif()
          continue()
        endif()
        get_target_property(alias "${candidate}" ALIASED_TARGET)
        if(alias)
          list(APPEND aliases "${candidate}=${alias}")
          set(candidate "${alias}")
        endif()
        list(APPEND dependencies "${candidate}")
      endforeach()
    endforeach()
    list(REMOVE_DUPLICATES dependencies)
    list(APPEND queue ${dependencies})
  endwhile()
  list(SORT origins)
  set(description)
  list(REMOVE_DUPLICATES aliases)
  foreach(alias IN LISTS aliases)
    string(REPLACE "=" "\t" alias "${alias}")
    string(APPEND description "ALIAS\t${alias}\n")
  endforeach()
  set(inputs "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SketchSDK.py"
             "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SketchFlags.py"
             "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SketchPlugin.cmake")
  foreach(origin IN LISTS origins)
    string(APPEND description "TARGET\t${origin}\n")
    string(APPEND description
      "DEPENDENCY\t$<JOIN:$<TARGET_GENEX_EVAL:${origin},$<TARGET_PROPERTY:${origin},SIGIL_SKETCH_NATIVE_PUBLIC_LINKS>>,\nDEPENDENCY\t>\n")
    get_target_property(sources "${origin}" SOURCES)
    get_target_property(source_directory "${origin}" SOURCE_DIR)
    get_target_property(include_roots "${origin}" INTERFACE_INCLUDE_DIRECTORIES)
    foreach(header IN LISTS sources)
      if(header MATCHES "\\.(h|hpp)$" AND NOT header MATCHES "\\$<")
        get_filename_component(header "${header}" ABSOLUTE BASE_DIR "${source_directory}")
        set(public FALSE)
        foreach(root IN LISTS include_roots)
          if(NOT root MATCHES "\\$<")
            string(FIND "${header}" "${root}/" prefix)
            if(prefix EQUAL 0)
              set(public TRUE)
            endif()
          endif()
        endforeach()
        if(public)
          string(APPEND description "HEADER\t${header}\n")
          list(APPEND inputs "${header}")
        endif()
      endif()
    endforeach()
    foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS COMPILE_FEATURES)
      string(APPEND description
        "${property}\t$<JOIN:$<TARGET_PROPERTY:${origin},INTERFACE_${property}>,\n${property}\t>\n")
    endforeach()
    get_target_property(imported "${origin}" IMPORTED)
    get_target_property(type "${origin}" TYPE)
    if(imported)
      # The package declares only the tree's own libraries; what an
      # imported origin requires is carried on the libraries that use it.
      string(APPEND description "IMPORTED\t1\n")
      if(type STREQUAL OBJECT_LIBRARY)
        string(APPEND description "BINARY\t$<JOIN:$<TARGET_OBJECTS:${origin}>,\nBINARY\t>\n")
      elseif(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|UNKNOWN_LIBRARY|EXECUTABLE)$")
        string(APPEND description "BINARY\t$<TARGET_FILE:${origin}>\n")
      endif()
      get_target_property(framework "${origin}" FRAMEWORK)
      if(framework)
        string(APPEND description "FRAMEWORK\t$<TARGET_FILE:${origin}>\n")
      endif()
    endif()
  endforeach()
  get_target_property(directory SigilSketch SIGIL_SKETCH_SDK_DIRECTORY)
  set(descriptor "${directory}/origins.txt")
  file(GENERATE OUTPUT "${descriptor}" CONTENT "${description}")
  set(flags "${directory}/native_flags.rsp")
  set(outputs "${flags}" "${directory}/SigilSketchBuildIdentity.h"
    "${directory}/boundaries.json" "${directory}/SigilSketchSDKConfig.cmake"
    "${directory}/SketchPlugin.cmake" "${directory}/SketchSDK.py"
    "${directory}/inputs.sha256" "${directory}/origin-inputs.json")
  add_custom_command(OUTPUT ${outputs}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SketchSDK.py"
      --compdb "${CMAKE_BINARY_DIR}/compile_commands.json"
      --anchor "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NativeSurface.cpp"
      --config "$<CONFIG>" --flags "${flags}" --out "${directory}"
      --origins "${descriptor}" --depfile "${directory}/metadata.d"
      --compiler "${CMAKE_CXX_COMPILER}" --compiler-id "${CMAKE_CXX_COMPILER_ID}"
      --compiler-version "${CMAKE_CXX_COMPILER_VERSION}"
      --architecture "${CMAKE_OSX_ARCHITECTURES}" --system-processor "${CMAKE_SYSTEM_PROCESSOR}"
    DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json" "${descriptor}"
            "${CMAKE_CXX_COMPILER}" ${inputs}
    DEPFILE "${directory}/metadata.d"
    COMMENT "Generating native sketch boundary metadata" VERBATIM)
  add_custom_target(sigil_sketch_metadata DEPENDS ${outputs})
  add_dependencies(SigilSketchSDK sigil_sketch_metadata)
  add_dependencies(sketch_flags SigilSketchSDK)
endfunction()
