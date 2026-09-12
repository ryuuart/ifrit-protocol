pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sigil.Sketchbook

Rectangle {
    id: navigation

    required property var rows
    required property string mode
    required property string selectedPath
    required property int count

    signal groupRequested(string path)
    signal modeRequested(string mode)
    signal branchToggled(string path)

    color: Theme.rail

    function toggle(path) {
        const y = tree.contentY;
        navigation.branchToggled(path);
        Qt.callLater(function() {
            tree.forceLayout();
            const index = navigation.rows.findIndex(function(row) { return row.path === path; });
            if (index >= 0)
                tree.currentIndex = index;
            tree.contentY = Math.max(tree.originY,
                Math.min(y, tree.originY + Math.max(0, tree.contentHeight - tree.height)));
        });
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        ComboBox {
            id: modeChoice

            Layout.fillWidth: true
            implicitHeight: 30
            model: ["Subjects", "Collections"]
            currentIndex: navigation.mode === "collections" ? 1 : 0
            font.pixelSize: 12
            Accessible.name: "Group sketches by"
            onActivated: navigation.modeRequested(currentIndex === 1 ? "collections" : "subjects")
        }

        ItemDelegate {
            id: allSketches

            Layout.fillWidth: true
            Layout.rightMargin: scrollbar.visible ? scrollbar.width : 0
            implicitHeight: 32
            topPadding: 0
            bottomPadding: 0
            rightPadding: 7
            text: "All sketches"
            highlighted: navigation.selectedPath.length === 0
            font.pixelSize: 12
            onClicked: navigation.groupRequested("")

            contentItem: RowLayout {
                Label {
                    Layout.fillWidth: true
                    text: "All sketches"
                    color: navigation.selectedPath.length ? Theme.label : Theme.text
                    font.pixelSize: 12
                }
                Label {
                    text: navigation.count
                    color: Theme.muted
                    font.family: Theme.mono
                    font.pixelSize: 10
                }
            }
            background: Rectangle {
                radius: 5
                color: allSketches.highlighted ? Theme.selection
                     : (allSketches.hovered ? Theme.hover : "transparent")
                border.width: 1
                border.color: allSketches.visualFocus ? Theme.accent : "transparent"
            }
        }

        // Qt's TreeView addresses a QAbstractItemModel through QModelIndex.
        // These groups are derived from JavaScript catalog rows, so ListView
        // presents their flattened branches without a second catalog model.
        ListView {
            id: tree

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: navigation.rows
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { id: scrollbar }

            // Arrow keys explore a branch; Enter chooses its results.
            Keys.onRightPressed: {
                const row = navigation.rows[tree.currentIndex];
                if (row?.branch && !row.expanded)
                    navigation.toggle(row.path);
            }
            Keys.onLeftPressed: {
                const row = navigation.rows[tree.currentIndex];
                if (!row)
                    return;
                if (row.branch && row.expanded) {
                    navigation.toggle(row.path);
                    return;
                }
                const parentPath = row.path.substring(0, row.path.lastIndexOf("/"));
                const parentIndex = navigation.rows.findIndex(function(candidate) {
                    return candidate.path === parentPath;
                });
                if (parentIndex >= 0)
                    tree.currentIndex = parentIndex;
            }
            Keys.onReturnPressed: {
                const row = navigation.rows[tree.currentIndex];
                if (row)
                    navigation.groupRequested(row.path);
            }
            Keys.onEnterPressed: {
                const row = navigation.rows[tree.currentIndex];
                if (row)
                    navigation.groupRequested(row.path);
            }

            delegate: ItemDelegate {
                id: group

                required property int index
                required property var modelData
                readonly property bool selected: navigation.selectedPath === modelData.path

                width: tree.width - (scrollbar.visible ? scrollbar.width : 0)
                height: 32
                topPadding: 0
                bottomPadding: 0
                leftPadding: 4 + modelData.depth * 12
                rightPadding: 7
                highlighted: selected
                Accessible.name: modelData.path + ", " + modelData.count + " sketches"
                onClicked: {
                    tree.currentIndex = index;
                    tree.forceActiveFocus();
                    navigation.groupRequested(modelData.path);
                }

                background: Rectangle {
                    radius: 5
                    color: group.selected ? Theme.selection
                         : (group.hovered || disclosure.hovered ? Theme.hover : "transparent")
                    border.width: 1
                    border.color: tree.activeFocus && tree.currentIndex === group.index
                        ? Theme.accent : "transparent"
                }

                contentItem: RowLayout {
                    spacing: 3

                    ToolButton {
                        id: disclosure

                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 26
                        padding: 0
                        text: group.modelData.branch ? (group.modelData.expanded ? "⌄" : "›") : ""
                        enabled: group.modelData.branch
                        opacity: group.modelData.branch ? 1 : 0
                        Accessible.ignored: !group.modelData.branch
                        font.pixelSize: 15
                        Accessible.name: (group.modelData.expanded ? "Collapse " : "Expand ") + group.modelData.label
                        background: Rectangle {
                            radius: 3
                            color: disclosure.down ? Theme.selection : "transparent"
                            border.width: 1
                            border.color: disclosure.visualFocus ? Theme.accent : "transparent"
                        }
                        onClicked: {
                            tree.currentIndex = group.index;
                            tree.forceActiveFocus();
                            navigation.toggle(group.modelData.path);
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: group.modelData.label
                        color: group.selected ? Theme.text
                             : (group.modelData.count ? Theme.label : Theme.faint)
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Label {
                        text: group.modelData.count
                        color: Theme.muted
                        font.family: Theme.mono
                        font.pixelSize: 10
                    }
                }

                ToolTip.visible: hovered
                ToolTip.delay: 700
                ToolTip.text: modelData.path + " · " + modelData.count + " sketches"
            }
        }
    }
}
