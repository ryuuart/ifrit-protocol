.pragma library

// A sketch can occupy several subject paths. Ancestors count each sketch
// once, even when two of its tags share the same parent.
function paths(sketch, mode) {
    if (mode === "collections")
        return [sketch.folder.split(" · ").join("/") || "Unfiled"];
    return sketch.tags && sketch.tags.length ? sketch.tags : ["Untagged"];
}

function contains(sketch, mode, path) {
    if (!path.length)
        return true;
    return paths(sketch, mode).some(function(candidate) {
        return candidate === path || candidate.startsWith(path + "/");
    });
}

function tree(sketches, mode, matching) {
    const hits = new Set(matching.map(function(sketch) { return sketch.sketchIndex; }));
    const roots = [];
    const nodes = Object.create(null);
    for (const sketch of sketches) {
        const counted = Object.create(null);
        for (const path of paths(sketch, mode)) {
            const parts = path.split("/");
            let parent = "";
            for (let depth = 0; depth < parts.length; ++depth) {
                const key = parent.length ? parent + "/" + parts[depth] : parts[depth];
                if (!nodes[key]) {
                    const node = { path: key, label: parts[depth], depth: depth,
                                   count: 0, children: [] };
                    nodes[key] = node;
                    (parent.length ? nodes[parent].children : roots).push(node);
                }
                if (hits.has(sketch.sketchIndex) && !counted[key]) {
                    ++nodes[key].count;
                    counted[key] = true;
                }
                parent = key;
            }
        }
    }
    function sort(branch) {
        branch.sort(function(a, b) { return a.label.localeCompare(b.label); });
        for (const node of branch)
            sort(node.children);
    }
    sort(roots);
    return roots;
}

function rows(tree, expanded, mode, searching) {
    const result = [];
    function visit(branch) {
        for (const node of branch) {
            const open = (searching && node.count > 0) || expanded[mode + ":" + node.path] === true;
            result.push({ path: node.path, label: node.label, depth: node.depth,
                          count: node.count, branch: node.children.length > 0,
                          expanded: open });
            if (open)
                visit(node.children);
        }
    }
    visit(tree);
    return result;
}
