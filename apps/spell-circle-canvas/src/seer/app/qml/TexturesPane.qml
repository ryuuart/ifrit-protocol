pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui
import Sigil.Seer

SplitView {
    id: pane
    required property TextureSources sources
    property string sourceName: ""
    property string sourceApplication: ""
    property string captureNote: ""
    property bool captureFailed: false
    orientation: Qt.Horizontal

    Ui.Panel {
        SplitView.preferredWidth: 300
        SplitView.minimumWidth: 250
        padding: 14
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Ui.PanelHeading {
                title: "Texture sources"
                detail: pane.sources.publications.length + " available"
                Layout.fillWidth: true
                Ui.IconButton { text: "↻"; tooltip: "Refresh texture sources"; onClicked: pane.sources.refresh() }
            }
            Label {
                Layout.fillWidth: true
                text: "Shared frames from Sketchbook, SpellCircle and other Syphon applications appear here."
                wrapMode: Text.WordWrap
                color: Ui.Theme.secondaryText
                font.pixelSize: Ui.Theme.bodySize
            }
            ListView {
                id: publications
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: pane.sources.publications
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    id: publicationRow
                    required property var modelData
                    width: ListView.view.width
                    highlighted: pane.sourceName === publicationRow.modelData.name && (!pane.sourceApplication || pane.sourceApplication === publicationRow.modelData.application)
                    onClicked: {
                        pane.sourceApplication = publicationRow.modelData.application;
                        pane.sourceName = publicationRow.modelData.name;
                        pane.captureNote = "";
                        preview.paused = false;
                    }
                    contentItem: ColumnLayout {
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            text: publicationRow.modelData.name
                            elide: Text.ElideRight
                            font.weight: Font.DemiBold
                            color: Ui.Theme.primaryText
                        }
                        Label {
                            Layout.fillWidth: true
                            text: publicationRow.modelData.application
                            elide: Text.ElideRight
                            font.pixelSize: Ui.Theme.captionSize
                            color: Ui.Theme.secondaryText
                        }
                    }
                }
                Label {
                    anchors.fill: parent
                    anchors.margins: 12
                    visible: publications.count === 0
                    text: pane.sources.supported ? "No publishers yet.\n\nStart a sketch and enable its texture publication, then select it here." : "Texture reception is available on macOS with Metal."
                    wrapMode: Text.WordWrap
                    color: Ui.Theme.secondaryText
                    font.pixelSize: Ui.Theme.bodySize
                }
            }
            Ui.SectionHeading { text: "Connect by name" }
            TextField {
                id: name
                Layout.fillWidth: true
                placeholderText: "Publication name"
                Accessible.name: "Texture publication name"
                onAccepted: connectByName.clicked()
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: application
                    Layout.fillWidth: true
                    placeholderText: "Application (optional)"
                    Accessible.name: "Publishing application"
                    onAccepted: connectByName.clicked()
                }
                Button {
                    id: connectByName
                    text: "Connect"
                    enabled: pane.sources.supported && name.text.trim().length > 0
                    onClicked: {
                        if (!enabled) return;
                        pane.sourceApplication = application.text.trim();
                        pane.sourceName = name.text.trim();
                        pane.captureNote = "";
                        preview.paused = false;
                    }
                }
            }
        }
    }

    Ui.Panel {
        SplitView.fillWidth: true
        SplitView.minimumWidth: 420
        padding: 14
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Ui.PanelHeading {
                Layout.fillWidth: true
                title: pane.sourceName || "Texture preview"
                detail: pane.sourceApplication || (pane.sourceName ? "Shared texture · any application" : "Select a source to inspect its frames")
                Button {
                    text: preview.paused ? "Resume" : "Pause"
                    enabled: pane.sourceName.length > 0
                    onClicked: preview.paused = !preview.paused
                }
                Button {
                    text: "Save PNG…"
                    enabled: preview.frameSize.width > 0
                    onClicked: capture.open()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Ui.Theme.viewportBackground
                radius: Ui.Theme.cornerRadius
                clip: true
                Ui.PanZoomCanvas {
                    id: viewport
                    anchors.fill: parent
                    canvasWidth: preview.frameSize.width > 0 ? preview.frameSize.width : 1
                    canvasHeight: preview.frameSize.height > 0 ? preview.frameSize.height : 1
                    showOverlays: false
                    checkerboardVisible: preview.frameSize.width > 0
                    TexturePreview {
                        id: preview
                        anchors.fill: parent
                        source: pane.sourceName
                        application: pane.sourceApplication
                        onFrameChanged: {
                            if (frameSize.width !== viewport.lastWidth || frameSize.height !== viewport.lastHeight) {
                                viewport.lastWidth = frameSize.width;
                                viewport.lastHeight = frameSize.height;
                                Qt.callLater(() => viewport.fitView());
                            }
                        }
                        onSaved: (path, success) => {
                            pane.captureFailed = !success;
                            pane.captureNote = success ? "Saved " + path : "Could not save " + path;
                        }
                    }
                    property int lastWidth: 0
                    property int lastHeight: 0
                }
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(400, parent.width - 48)
                    visible: preview.frameSize.width <= 0
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        text: pane.sourceName ? "Waiting for “" + pane.sourceName + "”" : "See what is being drawn"
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                        color: Ui.Theme.primaryText
                    }
                    Label {
                        Layout.fillWidth: true
                        text: pane.sourceName ? "The preview connects automatically when the publisher is available." : "Choose a texture source at the left. Inspect transparency, zoom into details and capture a full-resolution PNG."
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.pixelSize: Ui.Theme.bodySize
                        color: Ui.Theme.secondaryText
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Ui.StatusIndicator {
                    text: !pane.sourceName ? "No source selected" : preview.paused ? "Paused" : preview.status || "Connecting…"
                    tone: preview.connected ? "good" : "neutral"
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: preview.frameSize.width > 0 ? preview.frameSize.width + " × " + preview.frameSize.height + " · " + preview.frames + " frames" : ""
                    color: Ui.Theme.secondaryText
                    font.pixelSize: Ui.Theme.captionSize
                    font.family: Ui.Theme.monospaceFontFamily
                }
                ToolButton { text: "Fit"; enabled: preview.frameSize.width > 0; onClicked: viewport.fitView() }
                ToolButton { text: "100%"; enabled: preview.frameSize.width > 0; onClicked: viewport.zoomToActualSize() }
                ToolButton {
                    text: "Disconnect"
                    enabled: pane.sourceName.length > 0
                    onClicked: { pane.sourceName = ""; pane.sourceApplication = ""; }
                }
            }
            Ui.Notice {
                Layout.fillWidth: true
                visible: pane.captureNote.length > 0
                text: pane.captureNote
                tone: pane.captureFailed ? "error" : "good"
                dismissible: true
                onDismissed: pane.captureNote = ""
            }
        }
    }
    FileDialog {
        id: capture
        title: "Save texture frame"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)"]
        defaultSuffix: "png"
        onAccepted: preview.saveFrame(selectedFile)
    }
}
