# Which of several unpacked Adobe Substance 3D SDKs a build takes: the
# newest. A candidate's version is its directory name when that name is a
# version (`9.4.6`, `v9.4.6`), or the `-v<version>-` field of the
# download's own name (`substance-mac-arm64-v9.4.6-1a2b3c`); a directory
# renamed past both scores 0 and wins only when it is the only one. Kept
# apart from the find module so a script can ask the same question of a
# tree it lays out.

# substance_install_version(<name> <out>)
#   The version a candidate directory named <name> stands for, or 0.
function(substance_install_version name out)
  set(version 0)
  if(name MATCHES "^v?([0-9]+(\\.[0-9]+)*)$")
    set(version "${CMAKE_MATCH_1}")
  elseif(name MATCHES "-v([0-9]+(\\.[0-9]+)*)(-|$)")
    set(version "${CMAKE_MATCH_1}")
  endif()
  set(${out} "${version}" PARENT_SCOPE)
endfunction()

# substance_newest_install(<out> <root>...)
#   The newest directory directly under any <root> that holds a
#   substance-config.cmake, or empty when none does.
function(substance_newest_install out)
  set(best "")
  set(best_version "")
  foreach(root IN LISTS ARGN)
    file(GLOB candidates LIST_DIRECTORIES true "${root}/*")
    foreach(candidate IN LISTS candidates)
      if(NOT EXISTS "${candidate}/substance-config.cmake")
        continue()
      endif()
      get_filename_component(name "${candidate}" NAME)
      substance_install_version("${name}" version)
      if(best_version STREQUAL "" OR version VERSION_GREATER best_version)
        set(best "${candidate}")
        set(best_version "${version}")
      endif()
    endforeach()
  endforeach()
  set(${out} "${best}" PARENT_SCOPE)
endfunction()
