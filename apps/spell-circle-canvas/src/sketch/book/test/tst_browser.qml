import QtQuick
import QtTest
import "../qml" as Book

TestCase {
    id: test
    name: "SketchBrowser"

    property var browser

    Component {
        id: browserComponent
        Book.Browser {}
    }
    component View: QtObject {
        property real y: 0
        property int positioned: -1
        function scrollPosition() { return y; }
        function restoreScrollPosition(position) { y = position; }
        function positionAt(index) { positioned = index; }
    }
    View { id: list }
    View { id: gallery }

    readonly property var sketches: [
        { sketchIndex: 0, name: "A passage", key: "passage", folder: "Study · Type",
          kind: "canvas", blurb: "A paragraph", tags: ["Typography/Paragraph", "Typography/CJK", "Motion/Text"] },
        { sketchIndex: 1, name: "Brush", key: "brush", folder: "Draw · Procedural",
          kind: "draw", blurb: "Ink", tags: ["Drawing/Brushes"] },
        { sketchIndex: 2, name: "Letter", key: "letter", folder: "Catalog · Type",
          kind: "canvas", blurb: "A letter", tags: ["Typography/Lettering"] },
        { sketchIndex: 3, name: "Draft", key: "draft", folder: "Workspace",
          kind: "", blurb: "An external file", tags: [] }
    ]

    function init() {
        list.y = 0;
        gallery.y = 0;
        list.positioned = -1;
        gallery.positioned = -1;
        browser = createTemporaryObject(browserComponent, test, {
            catalog: { sketches: test.sketches }, listView: list, galleryView: gallery
        });
        verify(browser);
        browser.openOn(0);
        settle();
    }

    function settle() {
        wait(0);
        tryCompare(browser, "pendingScroll", false);
    }

    function node(path) {
        return browser.navigationRows.find(function(row) { return row.path === path; });
    }

    function test_overlappingTagsCountSketchesOnce() {
        compare(node("Typography").count, 2);
        browser.chooseGroup("Typography");
        settle();
        compare(browser.cards.length, 2);
        browser.chooseGroup("Motion/Text");
        settle();
        compare(browser.cards.length, 1);
        compare(browser.cards[0].sketchIndex, 0);
        verify(node("Motion/Text") !== undefined);
    }

    function test_expansionAndLearningKeepResultModelsStable() {
        const cards = browser.cards;
        const generation = browser.rebuildGeneration;
        browser.toggleGroup("Typography");
        browser.select(2);
        browser.overlayRow(Object.assign({}, sketches[2], { canvas: "900x600" }));
        compare(browser.cards, cards);
        compare(browser.rebuildGeneration, generation);
        compare(browser.selectedSketch.canvas, "900x600");
        verify(node("Typography/Paragraph") !== undefined);
    }

    function test_searchCountsAndGroupFilterCompose() {
        browser.chooseGroup("Drawing");
        browser.filterText = "tag:typography kind:canvas passage";
        settle();
        compare(browser.matchingCount, 1);
        compare(node("Typography").count, 1);
        compare(node("Drawing").count, 0);
        compare(browser.cards.length, 0);
        compare(browser.selectedIndex, -1);
        browser.chooseGroup("Typography");
        settle();
        compare(browser.cards.length, 1);
        browser.filterText = "tag:typography tag:motion";
        settle();
        compare(browser.cards.length, 1);
        browser.filterText = "cjk";
        settle();
        compare(browser.cards.length, 1);
    }

    function test_collectionsAndUntaggedRemainReachable() {
        browser.chooseGroup("Untagged");
        settle();
        compare(browser.cards[0].sketchIndex, 3);
        browser.chooseMode("collections");
        browser.chooseGroup("Study");
        settle();
        compare(browser.cards.length, 1);
        compare(browser.cards[0].sketchIndex, 0);
        browser.chooseGroup("Stu");
        settle();
        compare(browser.cards.length, 0);
    }

    function test_eachGroupKeepsBothScrollPositions() {
        list.y = 120;
        gallery.y = 240;
        browser.chooseGroup("Typography");
        settle();
        compare(list.y, 0);
        compare(gallery.y, 0);
        list.y = 35;
        gallery.y = 70;
        browser.chooseGroup("");
        settle();
        compare(list.y, 120);
        compare(gallery.y, 240);
        browser.chooseGroup("Typography");
        settle();
        compare(list.y, 35);
        compare(gallery.y, 70);
    }

    function test_invalidSavedGroupFallsBackAndNavigationUsesFilteredOrder() {
        browser.groupPath = "Removed";
        browser.openOn(0);
        settle();
        compare(browser.groupPath, "");
        browser.chooseGroup("Typography");
        settle();
        browser.select(0);
        browser.step(1);
        compare(browser.selectedIndex, 2);
        compare(list.positioned, 1);
        browser.viewMode = "gallery";
        browser.step(-1);
        compare(browser.selectedIndex, 0);
        compare(gallery.positioned, 0);
    }

    function test_switchingViewsPreservesSearchOrderSelectionAndPosition() {
        browser.chooseGroup("Typography");
        browser.filterText = "kind:canvas";
        browser.sortKey = "name";
        browser.sortAscending = false;
        settle();
        const cards = browser.cards;
        compare(cards.map(function(row) { return row.sketchIndex; }), [2, 0]);
        browser.select(0);
        list.y = 35;
        gallery.y = 70;
        const generation = browser.rebuildGeneration;
        browser.chooseView("gallery");
        settle();
        compare(browser.cards, cards);
        compare(browser.filterText, "kind:canvas");
        compare(browser.groupPath, "Typography");
        compare(browser.selectedIndex, 0);
        compare(browser.rebuildGeneration, generation);
        compare(gallery.y, 70);
        compare(gallery.positioned, -1);
        gallery.y = 95;
        browser.chooseView("list");
        settle();
        compare(list.y, 35);
        compare(gallery.y, 95);
        compare(list.positioned, -1);
        compare(browser.cards, cards);
    }

    function test_switchingDuringSearchLetsTheNewResultsRestoreTheirPosition() {
        gallery.y = 70;
        browser.filterText = "tag:typography";
        browser.chooseView("gallery");
        settle();
        compare(browser.cards.length, 2);
        compare(gallery.y, 0);
        compare(browser.filterText, "tag:typography");
    }

    function test_unknownSortValuesStayAlphabeticalAsSearchNarrows() {
        browser.catalog = { sketches: sketches.map(function(row) {
            return Object.assign({}, row, { moment: -1 });
        }) };
        browser.sortKey = "moment";
        browser.sortAscending = false;
        settle();
        compare(browser.cards.map(function(row) { return row.sketchIndex; }), [0, 1, 3, 2]);
        browser.filterText = "tag:typography";
        settle();
        compare(browser.cards.map(function(row) { return row.sketchIndex; }), [0, 2]);
        browser.chooseView("gallery");
        browser.filterText = "";
        settle();
        compare(browser.cards.map(function(row) { return row.sketchIndex; }), [0, 1, 3, 2]);
    }

    function test_clearFiltersResetsQueryAndGroupInEitherView_data() {
        return [{ tag: "list", mode: "list" }, { tag: "gallery", mode: "gallery" }];
    }

    function test_clearFiltersResetsQueryAndGroupInEitherView(data) {
        browser.chooseView(data.mode);
        browser.chooseGroup("Drawing");
        browser.filterText = "kind:canvas";
        settle();
        compare(browser.cards.length, 0);
        verify(browser.hasFilters);
        browser.clearFilters();
        settle();
        compare(browser.filterText, "");
        compare(browser.groupPath, "");
        compare(browser.cards.length, sketches.length);
        compare(browser.matchingCount, sketches.length);
        compare(browser.viewMode, data.mode);
        verify(!browser.hasFilters);
    }
}
