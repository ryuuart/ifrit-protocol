import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

/** Reusable width-by-height editor used by configuration forms. */
RowLayout {
    id: root

    required property int widthValue
    required property int heightValue
    property int from: 0
    property int to: 8192
    property int stepSize: 1
    property real fieldWidth: 140

    signal widthModified(int value)
    signal heightModified(int value)

    spacing: 6

    SpinBox {
        from: root.from
        to: root.to
        stepSize: root.stepSize
        editable: true
        Layout.preferredWidth: root.fieldWidth
        value: root.widthValue
        onValueModified: root.widthModified(value)
    }

    Label {
        text: "×"
    }

    SpinBox {
        from: root.from
        to: root.to
        stepSize: root.stepSize
        editable: true
        Layout.preferredWidth: root.fieldWidth
        value: root.heightValue
        onValueModified: root.heightModified(value)
    }
}
