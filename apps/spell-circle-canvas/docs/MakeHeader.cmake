# Doxygen's HTML header, with the theme's scripts spliced in.
#
# Run as a script, not included:
#
#     cmake -DDOXYGEN=<exe> -DWORK=<dir> -DOUT=<file> -P MakeHeader.cmake
#
# The header is GENERATED here rather than checked in. Doxygen emits the
# header its own version expects — the stock one already carries the
# viewport meta tag a phone needs — and a copy frozen in the source tree
# would silently drift from it on every Doxygen upgrade, which shows up
# as a half-styled page rather than an error. Generating means only the
# script tags below are ours.

foreach(var DOXYGEN WORK OUT)
  if(NOT ${var})
    message(FATAL_ERROR "MakeHeader.cmake: -D${var}=... is required")
  endif()
endforeach()

file(MAKE_DIRECTORY "${WORK}")
execute_process(
  COMMAND ${DOXYGEN} -w html header.html footer.html style.css
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE code
  OUTPUT_QUIET)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "doxygen -w html failed (${code})")
endif()

file(READ "${WORK}/header.html" header)

# $relpath^ is Doxygen's own placeholder for the path back to the output
# root, so one header works at every depth of the generated tree.
set(scripts [[
<script type="text/javascript" src="$relpath^doxygen-awesome-darkmode-toggle.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-fragment-copy-button.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-paragraph-link.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-interactive-toc.js"></script>
<script type="text/javascript">
  DoxygenAwesomeDarkModeToggle.init()
  DoxygenAwesomeFragmentCopyButton.init()
  DoxygenAwesomeParagraphLink.init()
  DoxygenAwesomeInteractiveToc.init()
</script>
</head>]])

string(FIND "${header}" "</head>" head_end)
if(head_end EQUAL -1)
  message(FATAL_ERROR "doxygen's generated header has no </head>")
endif()
string(REPLACE "</head>" "${scripts}" header "${header}")

file(WRITE "${OUT}" "${header}")
