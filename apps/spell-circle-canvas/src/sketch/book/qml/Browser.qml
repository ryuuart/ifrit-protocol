// Search, subject groups and ordering over stable registry rows.
// Selection moves the inspector; presenting remains a separate action.

import QtQuick
import "Groups.js" as Groups

QtObject {
    id: browser

    /** The rows every derivation starts from: the registry first, the
     *  files this run was pointed at after it. */
    required property var catalog
    /** The two views the rows are handed to, so a rebuild can keep the
     *  reader's place and a step can scroll to what it moved to. */
    property var listView
    property var galleryView

    property string viewMode: "list"
    property string sortKey: "name"
    property bool sortAscending: true
    property string filterText: ""
    property string groupMode: "subjects"
    property string groupPath: ""
    readonly property bool hasFilters: filterText.length > 0 || groupPath.length > 0
    readonly property var sortOptions: [
        { key: "name", label: "Name" },
        { key: "folder", label: "Collection" },
        { key: "kind", label: "Runtime" },
        { key: "canvas", label: "Canvas size" },
        { key: "moment", label: "Capture time" },
        { key: "lines", label: "Line count" }
    ]
    /** The row the inspector shows. Not the row the canvas presents:
     *  that is view.sketchIndex, and the two part company the moment
     *  someone starts browsing. */
    property int selectedIndex: -1
    property var cards: []
    /** Session-only facts keyed by registry index. They overlay the stable
     *  browser models so learning one canvas does not remount every thumbnail. */
    property var learnedSketches: ({})
    property int rebuildGeneration: 0

    onSortKeyChanged: Qt.callLater(browser.rebuild)
    onSortAscendingChanged: Qt.callLater(browser.rebuild)
    onFilterTextChanged: Qt.callLater(browser.rebuild)
    onGroupModeChanged: Qt.callLater(browser.rebuild)
    onGroupPathChanged: Qt.callLater(browser.rebuild)
    /** The inspector has a complete empty row when no sketch matches. */
    readonly property var blankSketch: ({
        sketchIndex: -1, name: "", key: "", folder: "", blurb: "", path: "",
        kind: "", available: true, reason: "", lines: 0, subject: "",
        editFirst: "", plate: "", canvas: "", background: "", moment: -1,
        videoExportable: false, tags: []
    })

    readonly property var selectedSketch:
        browser.learnedSketches[browser.selectedIndex]
            ?? browser.sketchAt(browser.selectedIndex) ?? browser.blankSketch

    function sketchAt(index) {
        const all = browser.catalog.sketches;
        for (let i = 0; i < all.length; ++i)
            if (all[i].sketchIndex === index)
                return all[i];
        return undefined;
    }

    // ---- Filtering -------------------------------------------------------
    //
    // Free words narrow on everything a sketch is written down as; a
    // `folder:`, `tag:` or `kind:` word narrows on that field alone. Every word
    // has to match, so words accumulate into one question rather than
    // widening it.

    function parseFilter(text) {
        let terms = { free: [], folder: [], kind: [], tag: [] };
        const words = text.trim().toLowerCase().split(/\s+/);
        for (let i = 0; i < words.length; ++i) {
            const word = words[i];
            if (word.length === 0)
                continue;
            if (word.startsWith("folder:"))
                terms.folder.push(word.substring(7));
            else if (word.startsWith("tag:"))
                terms.tag.push(word.substring(4));
            else if (word.startsWith("kind:"))
                terms.kind.push(word.substring(5));
            else
                terms.free.push(word);
        }
        return terms;
    }

    function matches(sketch, terms) {
        const hay = (sketch.name + " " + sketch.folder + " " + sketch.blurb
                     + " " + sketch.key + " " + (sketch.tags ?? []).join(" ")).toLowerCase();
        for (let i = 0; i < terms.free.length; ++i)
            if (hay.indexOf(terms.free[i]) < 0)
                return false;
        for (let i = 0; i < terms.folder.length; ++i)
            if (sketch.folder.toLowerCase().indexOf(terms.folder[i]) < 0)
                return false;
        for (let i = 0; i < terms.kind.length; ++i)
            if (sketch.kind.toLowerCase().indexOf(terms.kind[i]) < 0)
                return false;
        for (const tag of terms.tag)
            if (!(sketch.tags ?? []).some(function(path) { return path.toLowerCase().includes(tag); }))
                return false;
        return true;
    }

    // ---- Ordering --------------------------------------------------------

    function sortValue(sketch, key) {
        if (key === "folder")
            return sketch.folder + " " + sketch.name;
        if (key === "kind")
            return sketch.kind + " " + sketch.name;
        if (key === "lines")
            return sketch.lines;
        if (key === "moment")
            return sketch.moment;
        if (key === "canvas") {
            const size = /^(\d+)x(\d+)$/.exec(sketch.canvas);
            return size ? Number(size[1]) * Number(size[2]) : -1;
        }
        return sketch.name;
    }

    function compare(left, right) {
        const a = browser.sortValue(left, browser.sortKey);
        const b = browser.sortValue(right, browser.sortKey);
        // A fact a session has not answered yet sorts to the end either
        // way: it is not a small number, it is an unknown one.
        let order = 0;
        if (typeof a === "number" && typeof b === "number") {
            if (a < 0 && b >= 0) return 1;
            if (b < 0 && a >= 0) return -1;
            order = a - b;
        } else
            order = a.localeCompare(b);
        // Equal or unknown facts have a stable name order, so narrowing the
        // search cannot reshuffle the surviving rows among themselves.
        return order !== 0 ? (browser.sortAscending ? order : -order)
            : left.name.localeCompare(right.name) || left.sketchIndex - right.sketchIndex;
    }

    // The tree counts search hits before the selected group narrows them.
    // Each view remembers its place separately for every group and search.
    property var groupTree: []
    property var expandedGroups: ({})
    readonly property var navigationRows:
        Groups.rows(browser.groupTree, browser.expandedGroups,
                    browser.groupMode, browser.filterText.trim().length > 0)
    property int matchingCount: 0
    property var scrollPositions: ({})
    property string viewportKey: ""
    property bool pendingScroll: false

    function rebuild() {
        if (!browser.catalog || !browser.listView || !browser.galleryView)
            return;
        if (browser.viewportKey.length && !browser.pendingScroll)
            browser.scrollPositions[browser.viewportKey] = [
                browser.listView.scrollPosition(), browser.galleryView.scrollPosition()];
        const key = JSON.stringify([browser.groupMode, browser.groupPath, browser.filterText]);
        const position = browser.scrollPositions[key] ?? [0, 0];
        browser.viewportKey = key;
        browser.pendingScroll = true;
        const generation = ++browser.rebuildGeneration;
        const terms = browser.parseFilter(browser.filterText);
        const all = browser.catalog.sketches;
        const found = all.filter(function(sketch) { return browser.matches(sketch, terms); });
        browser.matchingCount = found.length;
        browser.groupTree = Groups.tree(all, browser.groupMode, found);
        const kept = found.filter(function(sketch) {
            return Groups.contains(sketch, browser.groupMode, browser.groupPath);
        });
        kept.sort(browser.compare);
        browser.cards = kept;
        if (browser.rowForSketch(browser.selectedIndex) < 0)
            browser.selectedIndex = kept.length ? kept[0].sketchIndex : -1;
        Qt.callLater(function() {
            if (generation !== browser.rebuildGeneration)
                return;
            browser.listView.restoreScrollPosition(position[0]);
            browser.galleryView.restoreScrollPosition(position[1]);
            browser.pendingScroll = false;
        });
    }

    function chooseGroup(path) {
        const next = Object.assign({}, browser.expandedGroups);
        const parts = path.split("/");
        for (let depth = 1; depth < parts.length; ++depth)
            next[browser.groupMode + ":" + parts.slice(0, depth).join("/")] = true;
        browser.expandedGroups = next;
        browser.groupPath = path;
    }

    function clearFilters() {
        browser.filterText = "";
        browser.groupPath = "";
    }

    function chooseMode(mode) {
        if (browser.groupMode === mode)
            return;
        browser.groupPath = "";
        browser.groupMode = mode;
    }

    function toggleGroup(path) {
        const next = Object.assign({}, browser.expandedGroups);
        const key = browser.groupMode + ":" + path;
        next[key] = !next[key];
        browser.expandedGroups = next;
    }

    function rowForSketch(index) {
        return browser.cards.findIndex(function(sketch) { return sketch.sketchIndex === index; });
    }

    function step(delta) {
        if (!browser.cards.length)
            return;
        const here = browser.rowForSketch(browser.selectedIndex);
        const to = Math.max(0, Math.min(browser.cards.length - 1,
                                       here < 0 ? 0 : here + delta));
        browser.selectedIndex = browser.cards[to].sketchIndex;
        browser.scrollToSelection();
    }

    function select(index) {
        browser.selectedIndex = index;
    }

    function chooseView(mode) {
        if (browser.viewMode === mode)
            return;
        const target = mode === "gallery" ? browser.galleryView : browser.listView;
        const position = target.scrollPosition();
        const generation = browser.rebuildGeneration;
        browser.viewMode = mode;
        Qt.callLater(function() {
            if (browser.viewMode === mode && generation === browser.rebuildGeneration)
                target.restoreScrollPosition(position);
        });
    }

    function scrollToSelection() {
        const at = browser.rowForSketch(browser.selectedIndex);
        if (at < 0)
            return;
        if (browser.viewMode === "gallery")
            browser.galleryView.positionAt(at);
        else
            browser.listView.positionAt(at);
    }

    function sortBy(key) {
        if (browser.sortKey === key)
            browser.sortAscending = !browser.sortAscending;
        else {
            browser.sortAscending = true;
            browser.sortKey = key;
        }
    }

    /** Overlays one row by its sketch index without disturbing the rest. */
    function overlayRow(row) {
        if (row.sketchIndex === undefined)
            return;
        const next = Object.assign({}, browser.learnedSketches);
        next[row.sketchIndex] = row;
        browser.learnedSketches = next;
    }

    function openOn(index) {
        // A removed tag must not leave a remembered group hiding the catalog.
        if (!browser.catalog.sketches.some(function(sketch) {
            return Groups.contains(sketch, browser.groupMode, browser.groupPath);
        }))
            browser.groupPath = "";
        browser.selectedIndex = index;
        browser.rebuild();
    }
}
