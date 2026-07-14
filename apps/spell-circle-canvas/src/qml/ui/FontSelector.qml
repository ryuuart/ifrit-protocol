import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

/**
 * Family, style, and size controls for a QFont-like value.
 *
 * fontDatabase must provide searchFamilies(), styles(), font(), and
 * styleForFont(). Modified values are emitted rather than written through so
 * the caller retains ownership of its configuration object.
 */
GridLayout {
    id: root

    required property var fontDatabase
    required property var selectedFont
    property real labelWidth: 100
    property real fieldWidth: 240
    property int minimumPointSize: 4
    property int maximumPointSize: 400

    signal fontModified(var fontValue)

    columns: 2
    columnSpacing: 14
    rowSpacing: 10

    Label {
        text: "Font Family"
        Layout.preferredWidth: root.labelWidth
    }
    FontFamilyField {
        Layout.preferredWidth: root.fieldWidth
        family: root.selectedFont.family
        searchFamilies: query => root.fontDatabase.searchFamilies(query)
        onFamilyChosen: family => {
            const availableStyles = root.fontDatabase.styles(family);
            const style = availableStyles.length > 0 ? availableStyles[0] : "Regular";
            root.fontModified(root.fontDatabase.font(family, style, root.selectedFont.pointSize));
        }
    }

    Label {
        text: "Font Style"
        Layout.preferredWidth: root.labelWidth
    }
    ComboBox {
        Layout.preferredWidth: root.fieldWidth
        model: root.fontDatabase.styles(root.selectedFont.family)
        currentIndex: model.indexOf(root.fontDatabase.styleForFont(root.selectedFont))
        onActivated: root.fontModified(root.fontDatabase.font(root.selectedFont.family, currentText, root.selectedFont.pointSize))
    }

    Label {
        text: "Font Size"
        Layout.preferredWidth: root.labelWidth
    }
    SpinBox {
        from: root.minimumPointSize
        to: root.maximumPointSize
        editable: true
        Layout.preferredWidth: 140
        value: root.selectedFont.pointSize
        onValueModified: {
            const updatedFont = root.selectedFont;
            updatedFont.pointSize = value;
            root.fontModified(updatedFont);
        }
    }
}
