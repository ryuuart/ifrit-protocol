import QtQuick
import QtQuick.Controls

Label {
    color: Theme.secondaryText
    font.pixelSize: Theme.captionSize
    font.weight: Font.DemiBold
    font.letterSpacing: 0.6
    elide: Text.ElideRight
    Accessible.role: Accessible.Heading
}
