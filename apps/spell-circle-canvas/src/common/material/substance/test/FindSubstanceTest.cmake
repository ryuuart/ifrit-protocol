# The find module's choice among several installs, asked of a tree laid
# out here: the newest wins whether a directory is named for its version,
# for its version behind a `v`, or keeps the download's own name, and a
# directory with no substance-config.cmake is never a candidate.
# Run as `cmake -DSUBSTANCE_INSTALL_MODULE=<file> -DSCRATCH=<dir> -P <this>`.
include(${SUBSTANCE_INSTALL_MODULE})

function(expect_newest label expected)
  substance_newest_install(chosen ${ARGN})
  get_filename_component(chosen "${chosen}" NAME)
  if(NOT chosen STREQUAL expected)
    message(FATAL_ERROR "${label}: chose '${chosen}', expected '${expected}'")
  endif()
endfunction()

function(install root name)
  file(MAKE_DIRECTORY "${root}/${name}")
  file(WRITE "${root}/${name}/substance-config.cmake" "")
endfunction()

file(REMOVE_RECURSE "${SCRATCH}")
set(plain "${SCRATCH}/plain")
install(${plain} 9.4.6)
install(${plain} 10.0.1)
install(${plain} 9.10.0)
expect_newest("plain versions" 10.0.1 ${plain})

set(prefixed "${SCRATCH}/prefixed")
install(${prefixed} 9.4.6)
install(${prefixed} v10.2.0)
expect_newest("a v before the version" v10.2.0 ${prefixed})

set(download "${SCRATCH}/download")
install(${download} 10.0.1)
install(${download} substance-mac-arm64-v10.1.0-4f2a9c)
expect_newest("a download's own name" substance-mac-arm64-v10.1.0-4f2a9c
              ${download})

set(roots "${SCRATCH}/roots")
install(${roots}/first 9.4.6)
install(${roots}/second 9.5.0)
file(MAKE_DIRECTORY "${roots}/first/11.0.0")
expect_newest("across roots, a bare directory ignored" 9.5.0
              ${roots}/first ${roots}/second)

file(REMOVE_RECURSE "${SCRATCH}")
