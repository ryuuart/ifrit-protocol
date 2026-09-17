import QtQuick
import QtTest
import Ifrit.Qt 1.0 as Ui

TestCase {
    id: tests
    name: "SharedControls"
    width: 500
    height: 300
    visible: true
    when: windowShown

    Component {
        id: segmentsComponent
        Ui.SegmentedControl {
            width: 300
            model: [{text: "One"}, {text: "Unavailable", enabled: false}, {text: "Three"}]
            onActivated: index => currentIndex = index
        }
    }
    Component { id: searchComponent; Ui.SearchField { width: 300 } }
    Component { id: buttonComponent; Ui.IconButton { text: "Open"; tooltip: "Open document" } }
    Component { id: sliderComponent; Ui.SliderField { width: 300; label: "Weight"; from: 0; to: 10 } }
    Component {
        id: fontFamilyComponent
        Ui.FontFamilyField {
            width: 300
            searchFamilies: query => ["Arial", "Avenir", "Alfa"].filter(
                family => family.toLowerCase().includes(query.toLowerCase()))
            onFamilyChosen: value => family = value
        }
    }
    Component {
        id: appWindowComponent
        Ui.AppWindow {
            id: sampleWindow
            width: 400
            height: 300
            visible: true
            property real simulatedTopInset: 36
            property real applicationHeaderHeight: 20
            SafeArea.additionalMargins.top: simulatedTopInset
            header: Item { height: sampleWindow.applicationHeaderHeight }
        }
    }

    function test_windowContentClearsSystemControlsAndApplicationHeader() {
        const window = createTemporaryObject(appWindowComponent, tests);
        verify(window);
        tryCompare(window, "topPadding", 36);
        compare(window.contentItem.y, 36);
        window.applicationHeaderHeight = 60;
        tryCompare(window, "topPadding", 60);
        compare(window.contentItem.y, 60);
        window.simulatedTopInset = 80;
        tryCompare(window, "topPadding", 80);
        compare(window.contentItem.y, 80);
        window.close();
    }

    Component {
        id: numberComponent
        Ui.NumberField {
            id: number
            width: 300
            label: "Stroke width"
            from: 0.5
            to: 100
            step: 0.5
            decimals: 1
            value: 4
            suffix: "px"
            onValueEdited: value => number.value = value
        }
    }
    SignalSpy { id: spy }

    function cleanup() {
        spy.target = null;
        spy.clear();
    }

    function test_programmaticSelectionIsNotAUserActivation() {
        const control = createTemporaryObject(segmentsComponent, tests);
        spy.signalName = "activated";
        spy.target = control;
        verify(waitForPolish(control));
        control.currentIndex = 2;
        compare(spy.count, 0);
        mouseClick(control, 150, control.height / 2);
        compare(spy.count, 0);
        mouseClick(control, 45, control.height / 2);
        compare(spy.count, 1);
        compare(control.currentIndex, 0);
        const first = control.contentItem.children.find(item => item.text === "One");
        const third = control.contentItem.children.find(item => item.text === "Three");
        verify(first.Accessible.selectable);
        verify(first.Accessible.selected);
        verify(!third.Accessible.selected);
        control.currentIndex = 2;
        verify(!first.Accessible.selected);
        verify(third.Accessible.selected);
    }

    function test_searchEscapeClearsAndReportsEdit() {
        const search = createTemporaryObject(searchComponent, tests);
        search.text = "brush";
        search.forceActiveFocus();
        spy.signalName = "textEdited";
        spy.target = search;
        keyClick(Qt.Key_Escape);
        compare(search.text, "");
        compare(spy.count, 1);
        verify(search.activeFocus);
    }

    function test_iconButtonCanBeActivatedFromTheKeyboard() {
        const button = createTemporaryObject(buttonComponent, tests);
        spy.signalName = "clicked";
        spy.target = button;
        button.forceActiveFocus();
        keyClick(Qt.Key_Space);
        compare(spy.count, 1);
        compare(button.Accessible.name, "Open document");
    }

    function test_sliderModelUpdatesDoNotWriteBack() {
        const slider = createTemporaryObject(sliderComponent, tests);
        spy.signalName = "valueEdited";
        spy.target = slider;
        slider.value = 8;
        compare(spy.count, 0);
        compare(slider.value, 8);
    }

    function test_numberModelUpdatesDoNotWriteBack() {
        const field = createTemporaryObject(numberComponent, tests);
        spy.signalName = "valueEdited";
        spy.target = field;
        field.value = 7.5;
        compare(spy.count, 0);
        compare(findChild(field, "numberInput").value, 75);
    }

    function test_numberAcceptsDecimalTextAndKeyboardSteps() {
        const field = createTemporaryObject(numberComponent, tests);
        const input = findChild(field, "numberInput");
        verify(waitForPolish(field));
        spy.signalName = "valueEdited";
        spy.target = field;
        input.contentItem.forceActiveFocus();
        input.contentItem.selectAll();
        keyClick(Qt.Key_2);
        keyClick(Qt.Key_Period);
        keyClick(Qt.Key_5);
        keyClick(Qt.Key_Return);
        compare(field.value, 2.5);
        verify(spy.count > 0);
        keyClick(Qt.Key_Up);
        compare(field.value, 3);
        compare(input.Accessible.name, "Stroke width");
    }

    function test_fontCandidatesKeepTypingFocusAndAcceptKeyboardSelection() {
        const field = createTemporaryObject(fontFamilyComponent, tests);
        const input = findChild(field, "fontFamilyInput");
        spy.signalName = "familyChosen";
        spy.target = field;
        input.forceActiveFocus();
        keyClick(Qt.Key_A);
        keyClick(Qt.Key_Down);
        keyClick(Qt.Key_Down);
        keyClick(Qt.Key_Up);
        keyClick(Qt.Key_Down);
        verify(input.activeFocus);
        compare(spy.count, 0);
        keyClick(Qt.Key_Return);
        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], "Avenir");
        compare(field.family, "Avenir");
        verify(input.activeFocus);
        compare(input.Accessible.name, "Font Family");
    }

    function test_fontQueryResetsTheKeyboardCandidate() {
        const field = createTemporaryObject(fontFamilyComponent, tests);
        const input = findChild(field, "fontFamilyInput");
        spy.signalName = "familyChosen";
        spy.target = field;
        input.forceActiveFocus();
        keyClick(Qt.Key_A);
        keyClick(Qt.Key_Down);
        keyClick(Qt.Key_Down);
        keyClick(Qt.Key_L);
        keyClick(Qt.Key_F);
        keyClick(Qt.Key_Return);
        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], "Alfa");
    }
}
