pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

/**
 * Searchable font-family field independent of a particular font database.
 *
 * Callers inject searchFamilies(query), allowing the field to work with a C++
 * font catalog, an application-owned model, or a fixed set of family names.
 */
Item {
    id: root

    property string family: ""
    property var searchFamilies: function (query) { return []; }

    signal familyChosen(string family)

    implicitWidth: 220
    implicitHeight: 32

    function stepCandidate(direction) {
        if (!candidatePopup.visible) {
            candidatePopup.candidates = root.searchFamilies(input.text);
            if (candidatePopup.candidates.length === 0)
                return;
            candidateList.currentIndex = direction < 0
                ? candidatePopup.candidates.length - 1 : 0;
            candidatePopup.open();
        } else if (candidateList.currentIndex < 0) {
            candidateList.currentIndex = direction < 0
                ? candidatePopup.candidates.length - 1 : 0;
        } else {
            candidateList.currentIndex = Math.max(0, Math.min(
                candidatePopup.candidates.length - 1,
                candidateList.currentIndex + direction));
        }
        candidateList.positionViewAtIndex(candidateList.currentIndex, ListView.Contain);
    }

    TextField {
        id: input
        objectName: "fontFamilyInput"
        anchors.fill: parent
        text: root.family
        placeholderText: "Search fonts…"
        Accessible.name: "Font Family"
        Keys.onDownPressed: root.stepCandidate(1)
        Keys.onUpPressed: root.stepCandidate(-1)
        Keys.onEscapePressed: candidatePopup.close()

        onTextEdited: {
            candidatePopup.candidates = root.searchFamilies(text);
            candidateList.currentIndex = -1;
            if (candidatePopup.candidates.length > 0)
                candidatePopup.open();
            else
                candidatePopup.close();
        }
        onAccepted: {
            if (candidatePopup.visible && candidatePopup.candidates.length > 0)
                root.familyChosen(candidatePopup.candidates[Math.max(0, candidateList.currentIndex)]);
            candidatePopup.close();
        }
        onActiveFocusChanged: {
            if (!activeFocus)
                candidatePopup.close();
        }
    }

    Popup {
        id: candidatePopup
        y: input.height
        width: input.width
        height: Math.min(220, Math.max(1, candidateList.contentHeight))
        padding: 0
        focus: false
        property var candidates: []

        contentItem: ListView {
            id: candidateList
            implicitHeight: contentHeight
            model: candidatePopup.candidates
            currentIndex: -1
            clip: true

            delegate: ItemDelegate {
                required property int index
                required property string modelData

                width: candidateList.width
                text: modelData
                highlighted: candidateList.currentIndex === index
                focusPolicy: Qt.NoFocus
                font.family: modelData
                font.pixelSize: 14
                Accessible.role: Accessible.ListItem
                Accessible.selectable: true
                Accessible.selected: highlighted
                onClicked: {
                    root.familyChosen(modelData);
                    candidatePopup.close();
                }
            }
        }
    }
}
