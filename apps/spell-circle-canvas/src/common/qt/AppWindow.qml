import QtQuick
import QtQuick.Controls as Controls
import Ifrit.Qt 1.0 as Ui

// Application content starts below system controls and any application header.
// Window flags and safe areas stay under Qt's platform geometry management.
Controls.ApplicationWindow {
    id: window

    color: Ui.Theme.windowBackground
    font.pixelSize: Ui.Theme.bodySize
    background: null
    topPadding: Math.max(SafeArea.margins.top,
                         (menuBar && menuBar.visible ? menuBar.height : 0)
                         + (header && header.visible ? header.height : 0))
    bottomPadding: Math.max(SafeArea.margins.bottom,
                            footer && footer.visible ? footer.height : 0)
    leftPadding: SafeArea.margins.left
    rightPadding: SafeArea.margins.right
}
