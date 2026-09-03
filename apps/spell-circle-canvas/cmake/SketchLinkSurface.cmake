# THE HOT-RELOAD LINK SURFACE, derived from the include surface.
#
# A sketch dylib links with `-undefined dynamic_lookup` and resolves the
# framework's symbols out of the host executable, so the host must
# CONTAIN them — which for a static archive means force-loading it
# whether or not the host's own translation units reference anything
# inside. Which archives? Exactly the ones a sketch may link, and what a
# sketch may link is what the sketch target links: what it may #include
# is that target's public dependencies, and the flags a hot-reloaded
# sketch compiles with are lifted from a source of that same target. So
# the archives are read off the same target rather than written down a
# second time — a list beside it would drift, and a missing archive is
# invisible everywhere but a dlopen: every sketch still compiles and
# every compiled-in sketch still runs, and the reloaded one fails with a
# symbol not found in the flat namespace, only for the symbols no
# compiled-in sketch happened to pull in.
#
# The walk covers the whole link closure — a private dependency of an
# archive rides in its interface as $<LINK_ONLY:…> and is an archive the
# host links all the same — and force-loads every archive of this
# repository's in it, by the prefix its targets carry. The vendored
# archives beneath them are resolved through those, except the ones a
# sketch calls directly, which the caller names as EXTRA.
#
# THE WALK RUNS LAST, deferred to the end of the top-level directory,
# and this is load-bearing: a link line may name a target that has not
# been defined yet, which the generator resolves later and a walk at
# configure time would silently drop — dropping precisely the archives
# whose directory is added after the host's. Deferred, every target in
# the tree exists, so a name the walk cannot resolve to a target is a
# plain library name and nothing of this repository's can go missing.
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
  get_target_property(queue ${sketches} INTERFACE_LINK_LIBRARIES)
  if(NOT queue)
    set(queue)
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
    "${host} force-loads ${count} archives off ${sketches}'s link surface")
endfunction()
