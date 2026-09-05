# sigil_qt_target(<target>...) — turn Qt's source scanning on for targets
# that carry Qt types.
#
# moc's scan is a per-target build step: every source of a target it is
# enabled on is read, and the target gains an autogen directory and an
# extra dependency edge whether or not a single Q_OBJECT is found. All but
# a handful of the targets in this tree are Qt-free, so the scan is off by
# default (decided in the top-level CMakeLists.txt) and named here by the
# targets that need it.
#
# Call this right after the target is created and BEFORE
# qt_add_qml_module(): a QML module registers its types from the JSON moc
# emits, so it fails to configure when its backing target is not already
# scanning.
#
# Only moc is turned on: no .ui or .qrc file exists here, since Qt Quick
# modules carry their own QML and resources.
function(sigil_qt_target)
  foreach(target IN LISTS ARGV)
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
  endforeach()
endfunction()
