import QtQuick
import QtQuick.Controls
import QtTest
import "../qml" as Book

TestCase {
    id: test
    name: "SketchPublication"
    when: windowShown
    width: 1000
    height: 80
    visible: true

    property bool requested: false

    Action {
        id: publication
        text: "Publish"
        shortcut: "Ctrl+P"
        checkable: true
        checked: test.requested
        onTriggered: {
            strip.publicationError = "";
            test.requested = checked;
        }
    }

    Book.StatusStrip {
        id: strip
        width: test.width
        publishAction: publication
    }

    function init() {
        test.requested = false;
        strip.publication = "";
        strip.publicationError = "";
    }

    function test_buttonAndShortcutUseTheSameAction() {
        const button = findChild(strip, "publicationButton");
        const status = findChild(strip, "publicationStatus");
        compare(status.text, "Publishing off");
        mouseClick(button);
        compare(test.requested, true);
        compare(status.busy, true);
        strip.publication = "Studio output";
        compare(status.text, "Publishing as Studio output");
        compare(status.tone, "good");
        keyClick(Qt.Key_P, Qt.ControlModifier);
        compare(test.requested, false);
        compare(button.checked, false);
        compare(status.text, "Publishing off");
    }

    function test_refusedPublicationCanBeRetriedFromTheVisibleControl() {
        const button = findChild(strip, "publicationButton");
        const status = findChild(strip, "publicationStatus");
        test.requested = true;
        strip.publicationError = "This graphics backend cannot publish frames.";
        test.requested = false;
        compare(button.checked, false);
        compare(status.text, "Publishing unavailable");
        compare(status.tone, "error");
        compare(status.busy, false);
        mouseClick(button);
        compare(test.requested, true);
        compare(strip.publicationError, "");
        compare(status.text, "Starting publication…");
    }
}
