// THE BROWSER'S MODEL: what the registry's rows are narrowed to, the
// order they are read in, and which of them are folded away.
//
// It is not the window. A filter, a sort column and a shut folder change
// which rows there are to look at and nothing about what the canvas is
// presenting — SELECTION is a look and ENTER is what moves the canvas —
// so the rows a view is handed are derived here and the window binds to
// them.

import QtQuick

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
    property string sortKey: "folder"
    property bool sortAscending: true
    property string filterText: ""
    property string folder: ""
    /** The row the inspector shows. Not the row the canvas presents:
     *  that is view.sketchIndex, and the two part company the moment
     *  someone starts browsing. */
    property int selectedIndex: -1
    property var collapsedGroups: ({})
    property var rows: []
    property var cards: []
    property var folders: []
    /** Session-only facts keyed by registry index. They overlay the stable
     *  browser models so learning one canvas does not remount every thumbnail. */
    property var learnedSketches: ({})
    property int rebuildGeneration: 0

    onSortKeyChanged: browser.rebuild()
    onSortAscendingChanged: browser.rebuild()
    onFilterTextChanged: browser.rebuild()
    onFolderChanged: browser.rebuild()
    /** Every field a row carries, empty — what a folder header stands
     *  in with, so a delegate never reads a field off the wrong kind of
     *  row and warns once per row per rebuild for nothing. */
    readonly property var blankSketch: ({
        sketchIndex: -1, name: "", key: "", folder: "", blurb: "", path: "",
        kind: "", available: true, reason: "", lines: 0, subject: "",
        editFirst: "", plate: "", canvas: "", background: "", moment: -1,
        videoExportable: false
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
    // `folder:` or `kind:` word narrows on that field alone. Every word
    // has to match, so words accumulate into one question rather than
    // widening it.

    function parseFilter(text) {
        let terms = { free: [], folder: [], kind: [] };
        const words = text.trim().toLowerCase().split(/\s+/);
        for (let i = 0; i < words.length; ++i) {
            const word = words[i];
            if (word.length === 0)
                continue;
            if (word.startsWith("folder:"))
                terms.folder.push(word.substring(7));
            else if (word.startsWith("kind:"))
                terms.kind.push(word.substring(5));
            else
                terms.free.push(word);
        }
        return terms;
    }

    function matches(sketch, terms) {
        const hay = (sketch.name + " " + sketch.folder + " " + sketch.blurb
                     + " " + sketch.key).toLowerCase();
        for (let i = 0; i < terms.free.length; ++i)
            if (hay.indexOf(terms.free[i]) < 0)
                return false;
        for (let i = 0; i < terms.folder.length; ++i)
            if (sketch.folder.toLowerCase().indexOf(terms.folder[i]) < 0)
                return false;
        for (let i = 0; i < terms.kind.length; ++i)
            if (sketch.kind.toLowerCase().indexOf(terms.kind[i]) < 0)
                return false;
        return true;
    }

    // ---- Ordering --------------------------------------------------------
    //
    // Ordering by FOLDER is the grouped reading, and is the default; any
    // other column is one flat run over the whole filtered set, because
    // a column you asked to be ordered by is one you want to read down
    // without folders interrupting it.

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
        if (typeof a === "number" && typeof b === "number") {
            if (a < 0 && b >= 0) return 1;
            if (b < 0 && a >= 0) return -1;
            return browser.sortAscending ? a - b : b - a;
        }
        return browser.sortAscending ? a.localeCompare(b) : b.localeCompare(a);
    }

    // ---- The rows --------------------------------------------------------

    function rebuild() {
        // Replacing either JavaScript-array model makes its view choose a
        // fresh contentY. Keep the reader's place across facts learned from
        // a newly presented sketch (and across any other registry rebuild).
        const listScroll = browser.listView.scrollPosition();
        const galleryScroll = browser.galleryView.scrollPosition();
        const generation = ++browser.rebuildGeneration;
        const terms = browser.parseFilter(browser.filterText);
        const all = browser.catalog.sketches;
        let found = [];
        for (let i = 0; i < all.length; ++i)
            if (browser.matches(all[i], terms))
                found.push(all[i]);

        // The chips count what the TEXT left, so a chip says how many
        // there are to switch to rather than how many are on screen.
        let counts = ({});
        let order = [];
        for (let i = 0; i < found.length; ++i) {
            if (counts[found[i].folder] === undefined) {
                counts[found[i].folder] = 0;
                order.push(found[i].folder);
            }
            ++counts[found[i].folder];
        }
        order.sort();
        let chips = [{ folder: "", label: "All", count: found.length }];
        for (let i = 0; i < order.length; ++i)
            chips.push({ folder: order[i], label: order[i],
                         count: counts[order[i]] });
        browser.folders = chips;

        let kept = [];
        for (let i = 0; i < found.length; ++i)
            if (browser.folder.length === 0 || found[i].folder === browser.folder)
                kept.push(found[i]);
        kept.sort(browser.compare);
        browser.cards = kept;

        let out = [];
        if (browser.sortKey === "folder") {
            let openFolder = "";
            let shut = false;
            for (let i = 0; i < kept.length; ++i) {
                if (kept[i].folder !== openFolder) {
                    openFolder = kept[i].folder;
                    // While filtering, a collapsed folder would hide its
                    // own hits — which is worse than no filter at all.
                    shut = terms.free.length === 0
                        && terms.folder.length === 0
                        && terms.kind.length === 0
                        && browser.collapsedGroups[openFolder] === true;
                    let count = 0;
                    for (let k = i; k < kept.length
                         && kept[k].folder === openFolder; ++k)
                        ++count;
                    out.push({ header: true, folder: openFolder, count: count,
                               collapsed: shut, sketch: browser.blankSketch });
                }
                if (!shut)
                    out.push({ header: false, folder: kept[i].folder,
                               count: 0, collapsed: false, sketch: kept[i] });
            }
        } else {
            for (let i = 0; i < kept.length; ++i)
                out.push({ header: false, folder: kept[i].folder, count: 0,
                           collapsed: false, sketch: kept[i] });
        }
        browser.rows = out;

        // A selection nothing on screen shows is not a selection: it
        // falls to the first row there is, which is the first row of the
        // first OPEN folder rather than of the first folder.
        if (browser.rowForSketch(browser.selectedIndex) < 0) {
            browser.selectedIndex = -1;
            const showing = browser.viewMode === "gallery" ? kept : out;
            for (let i = 0; i < showing.length; ++i) {
                const row = showing[i].header === undefined
                    ? showing[i] : (showing[i].header ? null : showing[i].sketch);
                if (row !== null) {
                    browser.selectedIndex = row.sketchIndex;
                    break;
                }
            }
        }

        // The views apply their new models on the next event-loop turn.
        // Only the latest rebuild gets to put the saved positions back.
        Qt.callLater(function() {
            if (generation !== browser.rebuildGeneration)
                return;
            browser.listView.restoreScrollPosition(listScroll);
            browser.galleryView.restoreScrollPosition(galleryScroll);
        });
    }

    function toggleGroup(name) {
        let next = ({});
        for (const key in browser.collapsedGroups)
            next[key] = browser.collapsedGroups[key];
        next[name] = !(next[name] === true);
        browser.collapsedGroups = next;
        browser.rebuild();
    }

    /** Where the selected sketch sits in whichever view is on, or -1
     *  when this one is not showing it. */
    function rowForSketch(index) {
        if (browser.viewMode === "gallery") {
            for (let i = 0; i < browser.cards.length; ++i)
                if (browser.cards[i].sketchIndex === index)
                    return i;
            return -1;
        }
        for (let i = 0; i < browser.rows.length; ++i)
            if (!browser.rows[i].header
                && browser.rows[i].sketch.sketchIndex === index)
                return i;
        return -1;
    }

    /** Moves the selection by @p step over what is actually on screen,
     *  stepping over folder headers, and scrolls it into view. */
    function step(delta) {
        const here = browser.rowForSketch(browser.selectedIndex);
        if (browser.viewMode === "gallery") {
            if (browser.cards.length === 0)
                return;
            const to = Math.max(0, Math.min(browser.cards.length - 1,
                                            (here < 0 ? 0 : here + delta)));
            browser.selectedIndex = browser.cards[to].sketchIndex;
            browser.galleryView.positionAt(to);
            return;
        }
        let i = here >= 0 ? here : (delta > 0 ? -1 : browser.rows.length);
        for (i += delta; i >= 0 && i < browser.rows.length; i += delta) {
            if (browser.rows[i].header)
                continue;
            browser.selectedIndex = browser.rows[i].sketch.sketchIndex;
            browser.listView.positionAt(i);
            return;
        }
    }

    function select(index) {
        browser.selectedIndex = index;
    }

    /** Brings the selection into view — opening the folder holding it
     *  first, because a selection inside a shut folder is one the list
     *  cannot show and the next rebuild would move.
     *
     *  The scroll waits for the rows to have been handed over: a view
     *  asked to hold an index it has not been given yet holds nothing,
     *  and the selection stays off screen with no sign that it did. */
    function reveal() {
        const sketch = browser.sketchAt(browser.selectedIndex);
        if (sketch === undefined)
            return;
        if (browser.viewMode === "list"
            && browser.collapsedGroups[sketch.folder] === true)
            browser.toggleGroup(sketch.folder);
        Qt.callLater(browser.scrollToSelection);
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
        let next = ({});
        for (const index in browser.learnedSketches)
            next[index] = browser.learnedSketches[index];
        next[row.sketchIndex] = row;
        browser.learnedSketches = next;
    }

    /** Opens on the SHAPE of the registry rather than on its first
     *  thirteen rows: every folder shut but the one holding what the
     *  canvas is presenting is one screen that says what is here. */
    function openOn(index) {
        const all = browser.catalog.sketches;
        const current = browser.sketchAt(index);
        let next = ({});
        for (let i = 0; i < all.length; ++i)
            next[all[i].folder] =
                current === undefined || all[i].folder !== current.folder;
        browser.collapsedGroups = next;
        browser.selectedIndex = index;
        browser.rebuild();
        browser.reveal();
    }
}
