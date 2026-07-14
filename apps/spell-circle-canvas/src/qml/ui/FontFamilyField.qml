pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

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

    TextField {
        id: input
        anchors.fill: parent
        text: root.family
        placeholderText: "Search fonts…"

        onTextEdited: {
            candidatePopup.candidates = root.searchFamilies(text);
            if (candidatePopup.candidates.length > 0)
                candidatePopup.open();
            else
                candidatePopup.close();
        }
        onAccepted: {
            if (candidatePopup.candidates.length > 0)
                root.familyChosen(candidatePopup.candidates[0]);
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
        property var candidates: []

        contentItem: ListView {
            id: candidateList
            implicitHeight: contentHeight
            model: candidatePopup.candidates
            clip: true

            delegate: ItemDelegate {
                required property string modelData

                width: candidateList.width
                text: modelData
                font.family: modelData
                font.pixelSize: 14
                onClicked: {
                    root.familyChosen(modelData);
                    candidatePopup.close();
                }
            }
        }
    }
}
