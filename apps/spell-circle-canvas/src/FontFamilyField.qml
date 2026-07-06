import QtQuick
import QtQuick.Controls.Basic
import SpellCircle.Models 1.0

// Searchable, autocompleting font-family picker. Typing filters the
// installed system fonts (via the FontDatabase singleton) and each result is
// previewed rendered in its own font; Return or a click commits the choice.
Item {
    id: root
    implicitWidth: 220
    implicitHeight: 32

    property string family: ""
    signal familyChosen(string family)

    TextField {
        id: input
        anchors.fill: parent
        text: root.family
        placeholderText: "Search fonts…"

        onTextEdited: {
            popup.candidates = FontDatabase.searchFamilies(text)
            if (popup.candidates.length > 0)
                popup.open()
            else
                popup.close()
        }
        onAccepted: {
            if (popup.candidates.length > 0)
                root.familyChosen(popup.candidates[0])
            popup.close()
        }
        onActiveFocusChanged: {
            if (!activeFocus)
                popup.close()
        }
    }

    Popup {
        id: popup
        y: input.height
        width: input.width
        height: Math.min(220, Math.max(1, list.contentHeight))
        padding: 0
        property var candidates: []

        contentItem: ListView {
            id: list
            implicitHeight: contentHeight
            model: popup.candidates
            clip: true

            delegate: ItemDelegate {
                width: list.width
                text: modelData
                font.family: modelData
                font.pixelSize: 14
                onClicked: {
                    root.familyChosen(modelData)
                    popup.close()
                }
            }
        }
    }
}
