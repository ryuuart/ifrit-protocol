# The hot-reload link surface: every archive of this repository's that
# the host links, force-loaded and re-exported.
#
# A sketch dylib links with `-undefined dynamic_lookup` and resolves the
# framework's symbols out of the host executable, so the host must
# contain them — which for a static archive means force-loading it
# whether or not the host's own translation units reference anything
# inside. A reloaded sketch must resolve every symbol a compiled-in one
# does, so the rule is every archive in both roots: the sketch target's
# closure (what a sketch may #include) and the host's own (the archives
# only an application brings up, a device backend among them). The
# closure is read off the targets rather than listed a second time,
# because a missing archive is invisible everywhere but a dlopen. A
# private dependency rides in an interface as $<LINK_ONLY:…> and is
# walked too; the vendored archives beneath are resolved through those,
# except the ones a sketch calls directly, which the caller names as
# EXTRA.
#
# The walk is deferred to the end of the top-level directory because a
# link line may name a target defined after the host's directory; walked
# then, a name that is not a target is a plain library name.
#
#   sigil_sketch_link_surface(<host> <sketches> [EXTRA <target>…])

function(sigil_sketch_link_surface host sketches)
  # A deferred call's arguments are expanded where it RUNS, and it runs
  # in the top-level directory, where nothing of this function's scope
  # exists — so the call is written out with its arguments already
  # spelled, and what is deferred is a line of literal text.
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\"
       CALL _sigil_sketch_link_surface_now ${host} ${sketches} ${ARGN})")
endfunction()

function(_sigil_sketch_link_surface_now host sketches)
  cmake_parse_arguments(SURFACE "" "" "EXTRA" ${ARGN})
  # Both roots at once: what the sketch target hands its consumers, and
  # what the host itself links. An executable keeps its own dependencies
  # in LINK_LIBRARIES and hands nothing to an interface, so the two
  # properties are different questions and both have to be asked.
  set(queue)
  get_target_property(surface ${sketches} INTERFACE_LINK_LIBRARIES)
  if(surface)
    list(APPEND queue ${surface})
  endif()
  get_target_property(linked ${host} LINK_LIBRARIES)
  if(linked)
    list(APPEND queue ${linked})
  endif()
  set(seen)
  while(queue)
    list(POP_FRONT queue item)
    string(REGEX REPLACE "^\\$<LINK_ONLY:(.+)>$" "\\1" item "${item}")
    # Anything else wrapped in a generator expression is not an archive a
    # sketch may name, and anything that is not a target is a plain
    # library name the linker already resolves.
    if(item MATCHES "\\$<" OR NOT TARGET ${item} OR item IN_LIST seen)
      continue()
    endif()
    get_target_property(aliased ${item} ALIASED_TARGET)
    if(aliased)
      list(APPEND queue ${aliased})
      continue()
    endif()
    list(APPEND seen ${item})
    get_target_property(interface ${item} INTERFACE_LINK_LIBRARIES)
    if(interface)
      list(APPEND queue ${interface})
    endif()
  endwhile()

  set(archives)
  foreach(candidate IN LISTS seen)
    get_target_property(type ${candidate} TYPE)
    if(type STREQUAL "STATIC_LIBRARY" AND candidate MATCHES "^Sigil")
      list(APPEND archives ${candidate})
    endif()
  endforeach()
  list(APPEND archives ${SURFACE_EXTRA})
  list(SORT archives)

  foreach(archive IN LISTS archives)
    target_link_options(${host} PRIVATE
      "LINKER:-force_load,$<TARGET_FILE:${archive}>")
  endforeach()
  list(LENGTH archives count)
  message(STATUS
    "${host} force-loads ${count} archives off its own and ${sketches}'s "
    "link surface")
endfunction()
