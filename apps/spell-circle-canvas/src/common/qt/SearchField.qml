import QtQuick
import QtQuick.Controls

TextField {
    id: field

    signal steppedOut

    implicitHeight: Theme.controlHeight
    leftPadding: 28
    rightPadding: clearButton.visible ? clearButton.width + 4 : 10
    placeholderText: "Search"
    selectByMouse: true
    Accessible.name: placeholderText
    Keys.onDownPressed: steppedOut()
    Keys.onEscapePressed: {
        if (text.length > 0) {
            field.clear();
            field.textEdited();
        }
    }
    Label {
        anchors.left: parent.left
        anchors.leftMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        text: "⌕"
        color: Theme.secondaryText
        Accessible.ignored: true
    }
    IconButton {
        id: clearButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.controlHeight
        height: field.height
        visible: field.text.length > 0
        text: "×"
        tooltip: "Clear search"
        onClicked: {
            field.clear();
            field.forceActiveFocus();
            field.textEdited();
        }
    }
}
