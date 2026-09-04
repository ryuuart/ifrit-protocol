// Delegates reach outward — the icons in the view toggle read which
// mode is on. Bound makes that capture explicit rather than resolved
// by scope-chain accident.
pragma ComponentBehavior: Bound

// The strip over everything: what is here, how to narrow it, and which
// of the two ways of looking at it is on.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sigil.Sketchbook

Rectangle {
    id: bar

    /** How many sketches there are, and how many the filter leaves. */
    property int total: 0
    property int shown: 0
    /** "list" or "gallery". Written here, read by the window. */
    property string viewMode: "list"
    property bool inspectorOpen: true
    property bool taskRunning: false
    property alias filterText: field.text

    signal viewModeRequested(string mode)
    signal inspectorToggled
    signal videoRequested
    /** The filter field gives up the keyboard downwards: typing narrows
     *  the list, and the arrow that follows should move in it. */
    signal steppedOut

    function focusFilter() {
        field.forceActiveFocus();
        field.selectAll();
    }

    implicitHeight: 52
    color: Theme.panel

    /** One half of the view toggle: a target the size of a click, lit
     *  when its mode is the one on. */
    component ModeButton: Rectangle {
        id: modeButton

        required property string mode

        width: 30
        height: 24
        radius: 5
        color: bar.viewMode === modeButton.mode ? Theme.border : "transparent"

        TapHandler {
            onTapped: bar.viewModeRequested(modeButton.mode)
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.rule
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 14
        spacing: 14

        Label {
            text: "SKETCHBOOK"
            color: Theme.warn
            font.pixelSize: 14
            font.bold: true
            font.letterSpacing: 1.1
        }
        Label {
            // The second number appears only when it differs, so the
            // line reads as a count until a filter makes it a fraction.
            text: bar.shown === bar.total
                ? bar.total + " sketches"
                : bar.total + " sketches · showing " + bar.shown
            color: Theme.faint
            font.family: Theme.mono
            font.pixelSize: 11
        }

        Rectangle {
            Layout.preferredWidth: 380
            Layout.maximumWidth: 380
            Layout.minimumWidth: 140
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: 7
            color: Theme.ground
            border.width: 1
            border.color: field.activeFocus ? Theme.accent : Theme.selection

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 5
                spacing: 5

                Label {
                    text: "⌕"
                    color: Theme.faint
                    font.pixelSize: 14
                }
                TextField {
                    id: field

                    Layout.fillWidth: true
                    placeholderText: "filter — name, folder, blurb, file, or folder: kind:"
                    color: Theme.text
                    placeholderTextColor: Theme.faintest
                    font.pixelSize: 12
                    background: null
                    padding: 0
                    // Escape clears rather than losing focus: the filter
                    // is a lens, and putting it down should be one key
                    // rather than select-all-delete.
                    Keys.onEscapePressed: field.text = ""
                    Keys.onDownPressed: bar.steppedOut()
                    Keys.onReturnPressed: bar.steppedOut()
                }
                ToolButton {
                    visible: field.text.length > 0
                    text: "×"
                    font.pixelSize: 15
                    implicitWidth: 22
                    implicitHeight: 22
                    onClicked: field.text = ""
                }
            }
        }

        Item { Layout.fillWidth: true }

        Button {
            text: "Export video"
            enabled: !bar.taskRunning
            implicitHeight: 28
            ToolTip.visible: hovered
            ToolTip.delay: 700
            ToolTip.text: "Export every available sketch as a vertical MP4"
            onClicked: bar.videoRequested()
        }

        // The two ways of looking at the same registry. A gallery
        // answers "which one was that", a list answers "what is here" —
        // and neither answers the other, which is why both are here.
        Rectangle {
            Layout.preferredWidth: 66
            Layout.preferredHeight: 28
            radius: 7
            color: Theme.ground
            border.width: 1
            border.color: Theme.selection

            Row {
                anchors.centerIn: parent
                spacing: 2

                ModeButton {
                    mode: "gallery"
                    Grid {
                        anchors.centerIn: parent
                        columns: 2
                        spacing: 3
                        Repeater {
                            model: 4
                            Rectangle {
                                width: 5
                                height: 5
                                color: bar.viewMode === "gallery"
                                    ? Theme.text : Theme.faint
                            }
                        }
                    }
                }
                ModeButton {
                    mode: "list"

                    Column {
                        anchors.centerIn: parent
                        spacing: 3
                        Repeater {
                            model: 3
                            Rectangle {
                                width: 13
                                height: 1.5
                                color: bar.viewMode === "list"
                                    ? Theme.text : Theme.faint
                            }
                        }
                    }
                }
            }
        }

        // The rail is a reading aid, not a fixture: a narrow window is
        // better spent on the canvas than on a column of facts already
        // read.
        ToolButton {
            text: bar.inspectorOpen ? "❯" : "❮"
            font.pixelSize: 11
            implicitWidth: 26
            implicitHeight: 26
            ToolTip.visible: hovered
            ToolTip.delay: 700
            ToolTip.text: bar.inspectorOpen ? "Close the inspector"
                                            : "Open the inspector"
            onClicked: bar.inspectorToggled()
        }
    }
}
