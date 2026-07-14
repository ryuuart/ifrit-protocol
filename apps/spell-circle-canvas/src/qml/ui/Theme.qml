pragma Singleton

import QtQuick

/** Shared visual tokens for Ifrit desktop tools and galleries. */
QtObject {
    readonly property color windowBackground: "#1a1a1a"
    readonly property color panelBackground: "#1e1e1e"
    readonly property color toolbarBackground: "#252526"
    readonly property color viewportBackground: "#1c1c1c"
    readonly property color canvasBackground: "#faf7f0"
    readonly property color border: "#2e2e2e"
    readonly property color primaryText: "#d4d4d4"
    readonly property color secondaryText: "#888888"
    readonly property color disabledText: "#606060"
    readonly property color statusText: "#6fbf8e"
    readonly property string monospaceFontFamily: "Menlo, Monaco, Courier New"
    readonly property int toolbarHeight: 38
}
