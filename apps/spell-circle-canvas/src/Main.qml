import QtQuick
import SpellCircle.Canvas 1.0
import SpellCircle.Models 1.0

Window {
    id: root
    width: 1280
    height: 800
    minimumWidth: 700
    minimumHeight: 480
    visible: true
    title: "Spell Circle"
    color: "#1a1a1a"

    property bool sidebarOpen: true

    SettingsWindow {
        id: settingsWindow
        visible: false
    }

    Item {
        anchors.fill: parent

        // ── Sidebar ──────────────────────────────────────────────────────────
        Rectangle {
            id: sidebar
            z: 1
            width: root.sidebarOpen ? 260 : 0
            height: parent.height
            color: "#1e1e1e"
            clip: true

            Behavior on width {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }

            // Fixed-width inner content so it doesn't squish during animation
            Item {
                width: 260
                height: parent.height

                Rectangle {
                    id: sidebarHead
                    width: parent.width
                    height: 38
                    color: "#252526"

                    Text {
                        anchors {
                            verticalCenter: parent.verticalCenter
                            left: parent.left
                            leftMargin: 12
                        }
                        text: "LOGS"
                        color: "#aaaaaa"
                        font.pixelSize: 11
                        font.letterSpacing: 1.5
                        font.weight: Font.DemiBold
                    }

                    Text {
                        anchors {
                            verticalCenter: parent.verticalCenter
                            right: parent.right
                            rightMargin: 10
                        }
                        text: feedView.count > 0 ? feedView.count : ""
                        color: "#555555"
                        font.pixelSize: 11
                    }
                }

                ListView {
                    id: feedView
                    anchors {
                        top: sidebarHead.bottom
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }
                    model: Models.spellCircleModel
                    clip: true

                    delegate: Item {
                        width: ListView.view.width
                        height: Math.max(48, logContent.implicitHeight + 16)

                        Column {
                            id: logContent
                            anchors {
                                left: parent.left
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 12
                                rightMargin: 8
                            }
                            spacing: 3

                            Text {
                                width: parent.width
                                text: message
                                color: "#d4d4d4"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                maximumLineCount: 2
                            }
                            Text {
                                text: timestamp + " · " + source
                                color: "#606060"
                                font.pixelSize: 10
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: "#2a2a2a"
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "Waiting for data…"
                        color: "#444444"
                        font.pixelSize: 13
                        visible: feedView.count === 0
                    }
                }

            }
        }

        // Sidebar / content divider line
        Rectangle {
            z: 1
            width: 1
            height: parent.height
            x: sidebar.width
            color: "#2e2e2e"
        }

        // ── Main canvas area ─────────────────────────────────────────────────
        Item {
            id: mainArea
            anchors {
                left: sidebar.right
                leftMargin: 1
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }

            readonly property int nativeW: Models.graphicsConfig.canvas.width
            readonly property int nativeH: Models.graphicsConfig.canvas.height
            property real viewScale: fitScale
            property real panX: 0.0
            property real panY: 0.0

            readonly property real fitScale: {
                var vw = canvasViewport.width - 60
                var vh = canvasViewport.height - 60
                if (vw <= 0 || vh <= 0) return 1.0
                return Math.min(vw / nativeW, vh / nativeH)
            }

            readonly property real minScale: 0.04
            readonly property real maxScale: 16.0

            function fitView() {
                viewScale = fitScale
                panX = 0
                panY = 0
            }

            function zoomTo100() {
                viewScale = 1.0
                panX = 0
                panY = 0
            }

            // Scales by `factor` while keeping the canvas point under
            // (px, py) — viewport-local coordinates — pinned in place.
            function zoomAt(factor, px, py) {
                var newScale = Math.max(minScale, Math.min(maxScale, viewScale * factor))
                var actual = newScale / viewScale
                if (actual === 1.0)
                    return
                var lx = px - canvasViewport.width  / 2 - panX
                var ly = py - canvasViewport.height / 2 - panY
                viewScale = newScale
                panX += lx * (1.0 - actual)
                panY += ly * (1.0 - actual)
            }

            // ── Top bar ──────────────────────────────────────────────────────
            Rectangle {
                id: topBar
                width: parent.width
                height: 38
                color: "#252526"
                z: 1

                // Sidebar toggle
                Rectangle {
                    id: sidebarToggle
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        leftMargin: 8
                    }
                    width: 28
                    height: 24
                    radius: 3
                    color: toggleHov.hovered ? "#404040" : "#2d2d2d"

                    Text {
                        anchors.centerIn: parent
                        text: root.sidebarOpen ? "◀" : "▶"
                        color: toggleHov.hovered ? "#dddddd" : "#888888"
                        font.pixelSize: 11
                    }

                    HoverHandler { id: toggleHov }
                    TapHandler { onTapped: root.sidebarOpen = !root.sidebarOpen }
                }

                Text {
                    anchors.centerIn: parent
                    text: "Preview"
                    color: "#666666"
                    font.pixelSize: 12
                }

                // Settings (gear) button
                Rectangle {
                    id: settingsToggle
                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: 8
                    }
                    width: 28
                    height: 24
                    radius: 3
                    color: settingsHov.hovered ? "#404040" : "#2d2d2d"

                    Text {
                        anchors.centerIn: parent
                        text: "⚙"
                        color: settingsHov.hovered ? "#dddddd" : "#888888"
                        font.pixelSize: 13
                    }

                    HoverHandler { id: settingsHov }
                    TapHandler {
                        onTapped: {
                            settingsWindow.visible = true
                            settingsWindow.raise()
                            settingsWindow.requestActivate()
                        }
                    }
                }
            }

            // ── Canvas viewport ───────────────────────────────────────────────
            Item {
                id: canvasViewport
                anchors {
                    top: topBar.bottom
                    left: parent.left
                    right: parent.right
                    bottom: bottomBar.top
                }
                clip: true

                // Viewport background
                Rectangle {
                    anchors.fill: parent
                    color: "#1c1c1c"
                }

                // Subtle grid dots to indicate infinite canvas space
                Item {
                    anchors.fill: parent
                    opacity: 0.25

                    Repeater {
                        model: Math.ceil(canvasViewport.width / 32) * Math.ceil(canvasViewport.height / 32)

                        Rectangle {
                            required property int index

                            property int col: index % Math.ceil(canvasViewport.width / 32)
                            property int row: Math.floor(index / Math.ceil(canvasViewport.width / 32))
                            x: col * 32 + (mainArea.panX % 32)
                            y: row * 32 + (mainArea.panY % 32)
                            width: 2
                            height: 2
                            color: "#666666"
                        }
                    }
                }

                // Canvas drop shadow
                Rectangle {
                    x: canvasDisplay.x + 6
                    y: canvasDisplay.y + 6
                    width: canvasDisplay.width
                    height: canvasDisplay.height
                    color: "#000000"
                    opacity: 0.55
                    radius: 1
                }

                // Checkerboard background for transparency, rendered in screen space
                // and clipped to the canvas bounds so it stays performant at any zoom level.
                Item {
                    x: canvasDisplay.x
                    y: canvasDisplay.y
                    width: canvasDisplay.width
                    height: canvasDisplay.height
                    clip: true

                    Canvas {
                        x: -parent.x
                        y: -parent.y
                        width: canvasViewport.width
                        height: canvasViewport.height
                        onPaint: {
                            var ctx = getContext("2d");
                            var s = 10;
                            ctx.fillStyle = "#cccccc";
                            ctx.fillRect(0, 0, width, height);
                            ctx.fillStyle = "#999999";
                            var cols = Math.ceil(width / s);
                            var rows = Math.ceil(height / s);
                            for (var row = 0; row < rows; row++) {
                                for (var col = (row % 2); col < cols; col += 2) {
                                    ctx.fillRect(col * s, row * s, s, s);
                                }
                            }
                        }
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                    }
                }

                // The canvas thumbnail/preview
                Item {
                    id: canvasDisplay
                    width: mainArea.nativeW * mainArea.viewScale
                    height: mainArea.nativeH * mainArea.viewScale
                    x: (canvasViewport.width  - width)  / 2 + mainArea.panX
                    y: (canvasViewport.height - height) / 2 + mainArea.panY

                    SpellCircle {
                        clip: true
                        alphaBlending: true
                        fillColor: "transparent"
                        anchors.fill: parent
                        model: Models.spellCircleModel
                        config: Models.graphicsConfig
                    }

                    // Canvas outline
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: "#3a3a3a"
                        border.width: 1
                    }
                }

                // Zoom level badge (top-right of viewport)
                Rectangle {
                    anchors {
                        top: parent.top
                        right: parent.right
                        topMargin: 10
                        rightMargin: 10
                    }
                    width: zoomBadgeTxt.width + 14
                    height: 22
                    radius: 3
                    color: "#252526"
                    opacity: 0.85

                    Text {
                        id: zoomBadgeTxt
                        anchors.centerIn: parent
                        text: Math.round(mainArea.viewScale * 100) + "%"
                        color: "#999999"
                        font.pixelSize: 11
                        font.family: "Menlo, Monaco, Courier New"
                    }
                }

                // Canvas size badge (bottom-right of canvas, visible when canvas is in view)
                Rectangle {
                    x: canvasDisplay.x + canvasDisplay.width - width - 2
                    y: canvasDisplay.y + canvasDisplay.height + 6
                    width: canvasSizeTxt.width + 12
                    height: 18
                    radius: 2
                    color: "#252526"
                    opacity: 0.7
                    visible: y + height < canvasViewport.height

                    Text {
                        id: canvasSizeTxt
                        anchors.centerIn: parent
                        text: mainArea.nativeW + " × " + mainArea.nativeH
                        color: "#666666"
                        font.pixelSize: 10
                        font.family: "Menlo, Monaco, Courier New"
                    }
                }

                // Middle-button pan handler
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.MiddleButton | Qt.LeftButton
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                    property point lastPos

                    onPressed: (mouse) => { lastPos = Qt.point(mouse.x, mouse.y) }
                    onPositionChanged: (mouse) => {
                        if (pressed) {
                            mainArea.panX += mouse.x - lastPos.x
                            mainArea.panY += mouse.y - lastPos.y
                            lastPos = Qt.point(mouse.x, mouse.y)
                        }
                    }
                }

                // Touchpad two-finger scroll → pan (pixelDelta is the smooth scroll value).
                // Mouse wheel → zoom toward cursor (discrete angleDelta, 120 units per notch).
                // Device type is checked explicitly because macOS can synthesize pixelDelta
                // for mouse wheel events, which would fool a pixelDelta-only check.
                WheelHandler {
                    target: null
                    acceptedDevices: PointerDevice.TouchPad
                    onWheel: (ev) => {
                        mainArea.panX += ev.pixelDelta.x
                        mainArea.panY += ev.pixelDelta.y
                    }
                }
                WheelHandler {
                    target: null
                    acceptedDevices: PointerDevice.Mouse
                    onWheel: (ev) => {
                        var factor = Math.pow(1.4, ev.angleDelta.y / 120.0)
                        mainArea.zoomAt(factor, ev.x, ev.y)
                    }
                }

                // Trackpad pinch-to-zoom (native macOS magnify gesture).
                PinchHandler {
                    target: null
                    property real scaleAtStart: 1.0
                    onActiveChanged: if (active) scaleAtStart = mainArea.viewScale
                    onActiveScaleChanged: {
                        if (!active)
                            return
                        var factor = (scaleAtStart * activeScale) / mainArea.viewScale
                        mainArea.zoomAt(factor, centroid.position.x, centroid.position.y)
                    }
                }
            }

            // ── Floating reset button ─────────────────────────────────────────
            Rectangle {
                z: 2
                anchors {
                    right: parent.right
                    bottom: bottomBar.top
                    rightMargin: 14
                    bottomMargin: 14
                }
                width: resetTxt.width + 18
                height: 28
                radius: 5
                color: resetHov.hovered ? "#3c3c3c" : "#252526"
                visible: feedView.count > 0

                Text {
                    id: resetTxt
                    anchors.centerIn: parent
                    text: "Reset"
                    color: resetHov.hovered ? "#dddddd" : "#888888"
                    font.pixelSize: 11
                }

                HoverHandler { id: resetHov }
                TapHandler { onTapped: Models.spellCircleModel.clear() }
            }

            // ── Bottom toolbar ────────────────────────────────────────────────
            Rectangle {
                id: bottomBar
                width: parent.width
                height: 32
                anchors.bottom: parent.bottom
                color: "#252526"

                // Left: canvas dimensions
                Text {
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        leftMargin: 12
                    }
                    text: mainArea.nativeW + " × " + mainArea.nativeH + " px"
                    color: "#4a4a4a"
                    font.pixelSize: 11
                    font.family: "Menlo, Monaco, Courier New"
                }

                // Right: zoom controls
                Row {
                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: 12
                    }
                    spacing: 6

                    // Zoom percentage
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Math.round(mainArea.viewScale * 100) + "%"
                        color: "#888888"
                        font.pixelSize: 12
                        font.family: "Menlo, Monaco, Courier New"
                        width: 46
                        horizontalAlignment: Text.AlignRight
                    }

                    // Separator
                    Rectangle {
                        width: 1
                        height: 16
                        color: "#353535"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Fit button
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 22
                        width: _fitTxt.width + 16
                        radius: 3
                        color: _fitHov.hovered ? "#3c3c3c" : "#2a2a2a"

                        Text {
                            id: _fitTxt
                            anchors.centerIn: parent
                            text: "Fit"
                            color: _fitHov.hovered ? "#dddddd" : "#aaaaaa"
                            font.pixelSize: 11
                        }

                        HoverHandler { id: _fitHov }
                        TapHandler { onTapped: mainArea.fitView() }
                    }

                    // 100% button
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 22
                        width: _100Txt.width + 16
                        radius: 3
                        color: _100Hov.hovered ? "#3c3c3c" : "#2a2a2a"

                        Text {
                            id: _100Txt
                            anchors.centerIn: parent
                            text: "100%"
                            color: _100Hov.hovered ? "#dddddd" : "#aaaaaa"
                            font.pixelSize: 11
                        }

                        HoverHandler { id: _100Hov }
                        TapHandler { onTapped: mainArea.zoomTo100() }
                    }
                }
            }
        }
    }
}
