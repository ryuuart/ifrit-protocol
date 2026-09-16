import QtQuick
import QtTest
import "../qml" as Book

TestCase {
    id: tests
    name: "SketchWorkspace"
    when: windowShown
    width: 600
    height: 800
    visible: true

    Book.WorkspaceWelcome {
        id: welcome
        anchors.fill: parent
        workspacePath: "/projects/art"
    }
    SignalSpy {
        id: opened
        target: welcome
        signalName: "openRequested"
    }

    function init() {
        welcome.sketchCount = 0;
        welcome.selectedSketch = ({});
        opened.clear();
    }

    function test_modulesOnlyExplainHowToDeclareAnEntry() {
        compare(findChild(welcome, "workspaceHeading").text, "No sketches found");
        verify(!findChild(welcome, "openWorkspaceEntry").visible);
        compare(opened.count, 0);
    }

    function test_multipleEntriesRequireAnExplicitOpen() {
        welcome.sketchCount = 2;
        welcome.selectedSketch = {
            sketchIndex: 12, entryPath: "src/art/scene.py", available: true
        };
        compare(findChild(welcome, "workspaceHeading").text, "Choose a sketch");
        compare(findChild(welcome, "selectedEntryPath").text, "src/art/scene.py");
        compare(opened.count, 0);
        mouseClick(findChild(welcome, "openWorkspaceEntry"));
        compare(opened.count, 1);
        compare(opened.signalArguments[0][0], 12);
    }

    function test_unavailableSelectionCannotOpen() {
        welcome.sketchCount = 2;
        welcome.selectedSketch = {
            sketchIndex: 12, entryPath: "src/art/scene.py", available: false
        };
        verify(!findChild(welcome, "openWorkspaceEntry").enabled);
        compare(opened.count, 0);
    }
}
