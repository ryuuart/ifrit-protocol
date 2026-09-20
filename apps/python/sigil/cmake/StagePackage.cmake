# Put the package's own modules beside the declarations generated for them.
# Run with -DSOURCE=<package> -DDESTINATION=<staged package>.
#
# The declarations are written into the same directory by the pass that
# follows the extension's link, so this cannot simply replace the directory.
# It removes the modules the package no longer has instead: a module that
# moved would otherwise stay behind and be imported in place of the one that
# replaced it.
file(GLOB_RECURSE staged RELATIVE "${DESTINATION}" "${DESTINATION}/*.py")
foreach(module IN LISTS staged)
  if(NOT EXISTS "${SOURCE}/${module}")
    file(REMOVE "${DESTINATION}/${module}")
  endif()
endforeach()
file(COPY "${SOURCE}/" DESTINATION "${DESTINATION}")
