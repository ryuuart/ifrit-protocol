// The one palette. Every panel in this app is painted from here, so a
// colour is named for the JOB it does rather than for the hue it is —
// which is what lets the whole window be re-tuned in one file.
pragma Singleton

import QtQuick

QtObject {
    // Grounds, darkest first: the window behind everything, the panels
    // on it, and the rail that has to read as further back than they do.
    readonly property color ground: "#0a0912"
    readonly property color panel: "#12101e"
    readonly property color rail: "#0d0b17"

    // A row under the pointer, a row that is selected, and the outline
    // that says which row the canvas is actually presenting.
    readonly property color hover: "#181530"
    readonly property color selection: "#1e1a33"
    readonly property color accent: "#5b4bb0"

    // Lines: the one that separates panels, and the quieter one inside
    // a panel.
    readonly property color border: "#3a3366"
    readonly property color rule: "#181530"
    readonly property color ruleSoft: "#12101e"

    // Text, brightest first. `faint` is for the labels a reader looks
    // for rather than at — column heads, key hints, counts.
    readonly property color text: "#e8ecf8"
    readonly property color value: "#d3dbf0"
    readonly property color muted: "#767e99"
    readonly property color label: "#8f98b2"
    readonly property color faint: "#5c6480"
    readonly property color faintest: "#4d5470"

    // The three answers a fact can carry: good, wanting, wrong.
    readonly property color good: "#5bd47a"
    readonly property color warn: "#ffb46b"
    readonly property color bad: "#ff9aa4"

    // Numbers line up only in a fixed pitch, and every number in this
    // window is one a reader compares against the number above it.
    readonly property string mono: "Menlo"
}
