pragma Singleton

import QtQuick

/**
 * Shared visual tokens for Ifrit desktop tools and galleries.
 *
 * Every chrome color derives from the system palette so windows follow the
 * OS light/dark appearance, and surfaces carry alpha so they read as tinted
 * glass over a vibrant window (WindowChrome.applyVibrancy) while still
 * degrading to sensible solids on an opaque background.
 */
QtObject {
    id: root

    readonly property SystemPalette system: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    readonly property bool darkMode: root.system.window.hslLightness < 0.5

    /** Opaque fallback for windows where vibrancy is unavailable. */
    readonly property color windowBackground: root.system.window

    /** Translucent surface tints layered over the window background. */
    readonly property color panelBackground: Qt.rgba(root.system.window.r, root.system.window.g, root.system.window.b, 0.35)
    readonly property color toolbarBackground: Qt.rgba(root.system.window.r, root.system.window.g, root.system.window.b, 0.5)

    /** Opaque pasteboard behind the pan-and-zoom canvas viewport. */
    readonly property color viewportBackground: Qt.darker(root.system.window, root.darkMode ? 1.25 : 1.12)

    /** Paper color of the rendered text canvas itself (content, not chrome). */
    readonly property color canvasBackground: "#faf7f0"

    readonly property color border: Qt.rgba(root.system.windowText.r, root.system.windowText.g, root.system.windowText.b, 0.16)
    readonly property color primaryText: root.system.windowText
    readonly property color secondaryText: Qt.rgba(root.system.windowText.r, root.system.windowText.g, root.system.windowText.b, 0.6)
    readonly property color disabledText: Qt.rgba(root.system.windowText.r, root.system.windowText.g, root.system.windowText.b, 0.35)
    readonly property color statusText: root.darkMode ? "#6fbf8e" : "#1e7a45"

    /** Subtle interactive-surface tints (hoverable chips, toolbar buttons). */
    readonly property color controlBackground: Qt.rgba(root.system.windowText.r, root.system.windowText.g, root.system.windowText.b, 0.07)
    readonly property color controlHoverBackground: Qt.rgba(root.system.windowText.r, root.system.windowText.g, root.system.windowText.b, 0.14)

    readonly property string monospaceFontFamily: "Menlo, Monaco, Courier New"
    readonly property int toolbarHeight: 38
}
