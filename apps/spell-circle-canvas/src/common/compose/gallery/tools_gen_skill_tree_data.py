#!/usr/bin/env python3
"""Regenerate SkillTreeData.h from Path of Exile's passive-tree export.

Run it from anywhere; it caches the export next to itself and rewrites
SkillTreeData.h in place:

    python3 tools_gen_skill_tree_data.py

What is taken from the export is GEOMETRY — group centres, the orbit-radius
ladder, orbitIndex -> angle, node kinds, and which links are same-orbit arcs
rather than straight spokes. Names and stat wording are ours, so no game
copy ends up in the repository.
"""
import collections
import json
import math
import pathlib
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
EXPORT_URL = ("https://raw.githubusercontent.com/grindinggear/"
              "skilltree-export/master/data.json")
SRC = HERE / ".skilltree-export.json"   # cached, gitignored
OUT = HERE / "SkillTreeData.h"

if not SRC.exists():
    print("downloading", EXPORT_URL)
    with urllib.request.urlopen(EXPORT_URL) as response:
        SRC.write_bytes(response.read())
SEED = "23090"   # Call to Arms
RADIUS = 1500.0  # tree units around the seed

# Stage geometry (kSceneSize) and the margin the tree keeps off the edges.
STAGE_W, STAGE_H = 900.0, 640.0
MARGIN_X, MARGIN_Y = 62.0, 58.0

d = json.loads(SRC.read_text())
C = d["constants"]
RAD = C["orbitRadii"]
SPO = C["skillsPerOrbit"]
groups, nodes = d["groups"], d["nodes"]


def pos(nd):
    g = groups.get(str(nd.get("group")))
    if not g:
        return None
    o, oi = nd.get("orbit", 0), nd.get("orbitIndex", 0)
    a = 2 * math.pi * oi / max(SPO[o] if o < len(SPO) else 1, 1)
    r = RAD[o] if o < len(RAD) else 0
    return (g["x"] + r * math.sin(a), g["y"] - r * math.cos(a))


adj = collections.defaultdict(set)
for nid, nd in nodes.items():
    for other in nd.get("out", []) + nd.get("in", []):
        adj[nid].add(other)
        adj[other].add(nid)


def eligible(nid):
    nd = nodes.get(nid)
    return (nd and not nd.get("ascendancyName") and not nd.get("isProxy")
            and nd.get("classStartIndex") is None and nd.get("group") is not None)


# Everything within RADIUS of the seed that is still reachable from it: a
# disc of the real tree, not a node-count-capped crawl that would wander.
seedPos = pos(nodes[SEED])
inDisc = {n for n in nodes
          if eligible(n) and pos(nodes[n])
          and math.dist(pos(nodes[n]), seedPos) <= RADIUS}
order, q = [SEED], collections.deque([SEED])
seen = {SEED}
while q:
    for nb in sorted(adj[q.popleft()]):
        if nb in inDisc and nb not in seen:
            seen.add(nb)
            order.append(nb)
            q.append(nb)
order.sort(key=lambda n: (pos(nodes[n])[1], pos(nodes[n])[0]))
index = {n: i for i, n in enumerate(order)}

# ---- scene-space transform (uniform scale, centred) -----------------------
pts = {n: pos(nodes[n]) for n in order}
xs, ys = [p[0] for p in pts.values()], [p[1] for p in pts.values()]
spanX, spanY = max(xs) - min(xs), max(ys) - min(ys)
scale = min((STAGE_W - 2 * MARGIN_X) / spanX, (STAGE_H - 2 * MARGIN_Y) / spanY)
offX = (STAGE_W - spanX * scale) / 2 - min(xs) * scale
offY = (STAGE_H - spanY * scale) / 2 - min(ys) * scale


def to_scene(p):
    return (p[0] * scale + offX, p[1] * scale + offY)


# ---- the allocated run: a real shortest path in this subgraph -------------
sub = {n: {m for m in adj[n] if m in index} for n in order}


def bfs_path(src, dst):
    prev, frontier = {src: None}, collections.deque([src])
    while frontier:
        cur = frontier.popleft()
        if cur == dst:
            break
        for nb in sorted(sub[cur]):
            if nb not in prev:
                prev[nb] = cur
                frontier.append(nb)
    if dst not in prev:
        return []
    path, cur = [], dst
    while cur is not None:
        path.append(cur)
        cur = prev[cur]
    return path[::-1]


keystones = [n for n in order if nodes[n].get("isKeystone")]
notables = [n for n in order if nodes[n].get("isNotable")]
# The spine a player would actually have walked: the longest run that ENDS
# at a keystone, starting from whichever notable is furthest from it.
best = []
for a in keystones:
    for b in (notables or order):
        run = bfs_path(b, a)
        if len(run) > len(best):
            best = run
allocated = set(best)
frontier = {m for n in allocated for m in sub[n]} - allocated

# ---- names ---------------------------------------------------------------
# Our wording, in the genre's register; the export supplies no text here.
KEYSTONE_TEXT = [
    ("EMBERWARDEN", ["Fire damage you take is dealt over 4 seconds",
                     "You cannot be Ignited"],
     "The first flame asked only to be carried."),
    ("HOLLOW COVENANT", ["Life recovery applies to Energy Shield instead",
                         "Maximum Life is halved"],
     "Trade the vessel; keep the flame."),
    ("STILLPOINT", ["Skills cost no Mana while stationary",
                    "You cannot regenerate Mana while moving"],
     "The world turns. You need not."),
]
NOTABLE_TEXT = [
    ("Kindled Reserve", ["18% increased Fire Damage",
                         "+12 to maximum Life"]),
    ("Cinderpath", ["20% increased Damage over Time",
                    "10% increased Area of Effect"]),
    ("Ashen Discipline", ["+16% to Fire Resistance",
                          "6% increased maximum Life"]),
    ("Wickwright", ["25% increased Ignite Duration",
                    "12% increased Burning Damage"]),
    ("Tinderheart", ["+20 to maximum Life",
                     "8% increased Attack Speed while on Low Life"]),
    ("Long Watch", ["+8% to all Elemental Resistances",
                    "15% increased Stun Threshold"]),
    ("Sootbound", ["30% increased Armour",
                   "5% additional Physical Damage Reduction"]),
    ("Emberglass", ["+40 to maximum Energy Shield",
                    "10% increased Energy Shield Recharge Rate"]),
]
MINOR_TEXT = [
    ("Fire Damage", ["10% increased Fire Damage"]),
    ("Life", ["+10 to maximum Life"]),
    ("Resistance", ["+8% to Fire Resistance"]),
    ("Burning", ["12% increased Burning Damage"]),
    ("Armour", ["14% increased Armour"]),
    ("Energy Shield", ["+16 to maximum Energy Shield"]),
    ("Elemental Damage", ["8% increased Elemental Damage"]),
    ("Cast Speed", ["4% increased Cast Speed"]),
]

KIND = {"minor": 0, "notable": 1, "keystone": 2, "jewel": 3, "mastery": 4}


def kind_of(nd):
    if nd.get("isKeystone"):
        return "keystone"
    if nd.get("isNotable"):
        return "notable"
    if nd.get("isMastery"):
        return "mastery"
    if nd.get("isJewelSocket"):
        return "jewel"
    return "minor"


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


# ---- groups used, in scene space ------------------------------------------
used_groups = []
group_index = {}
for n in order:
    g = nodes[n]["group"]
    if g not in group_index:
        group_index[g] = len(used_groups)
        used_groups.append(g)

lines = []
w = lines.append
w("#pragma once")
w("// Passive-tree geometry lifted from Path of Exile's public tree export")
w("// (grindinggear/skilltree-export data.json): group centres, the")
w("// orbitRadii/skillsPerOrbit ladder, orbitIndex -> angle, node kinds, and")
w("// which links are same-group/same-orbit ARCS rather than straight spokes.")
w("// Generated, then frozen — this is a study fixture, not a live import.")
w("//")
w("// Constants from that export, verbatim:")
w("//   orbitRadii     %s" % RAD)
w("//   skillsPerOrbit %s" % SPO)
w("//")
w("// The cluster is every node within %.0f tree units of one keystone that" % RADIUS)
w("// is still connected to it (%d of them), transformed" % len(order))
w("// into the 900x640 stage with a uniform scale (%.5f) so the orbit" % scale)
w("// proportions survive. Node names and stat wording are ours; the export")
w("// carries its own and we are not reproducing it.")
w("")
w("#include <cstdint>")
w("")
w("namespace compose_gallery::skill_tree_data {")
w("")
w("enum class Kind : uint8_t { Minor, Notable, Keystone, Jewel, Mastery };")
w("enum class State : uint8_t { Unallocated, CanAllocate, Allocated };")
w("")
w("struct Node {")
w("  float x, y;        ///< scene space (900x640)")
w("  uint8_t group;     ///< index into kGroups")
w("  uint8_t orbit;     ///< index into the orbitRadii ladder")
w("  Kind kind;")
w("  State state;")
w("  const char *name;  ///< empty for anonymous minors")
w("};")
w("")
w("struct Group {")
w("  float x, y;            ///< scene-space centre")
w("  float radius[7];       ///< occupied orbit radii, scene space, 0 = unused")
w("};")
w("")
w("struct Edge {")
w("  uint8_t a, b;      ///< indices into kNodes")
w("  bool arc;          ///< same group + same orbit: follows the circle")
w("  uint8_t group;     ///< arc centre (index into kGroups), 0xFF for spokes")
w("};")
w("")
w("/** One tooltip's worth of copy for the named nodes. */")
w("struct Detail {")
w("  const char *name;")
w("  const char *kind;")
w("  const char *stats[3];   ///< nullptr-terminated")
w("  const char *flavour;    ///< keystones only")
w("};")
w("")

# groups with their occupied orbit radii in scene space
w("inline constexpr Group kGroups[] = {")
for g in used_groups:
    gx, gy = to_scene((groups[str(g)]["x"], groups[str(g)]["y"]))
    orbits = sorted({nodes[n].get("orbit", 0) for n in order if nodes[n]["group"] == g})
    radii = [0.0] * 7
    for o in orbits:
        if 0 < o < len(RAD):
            radii[o] = RAD[o] * scale
    w("    {%.2ff, %.2ff, {%s}}," % (gx, gy, ", ".join("%.2ff" % r for r in radii)))
w("};")
w("")

# names: keystones and notables get real labels, minors get a category word
ks_iter, nt_iter, mn_iter = iter(KEYSTONE_TEXT), iter(NOTABLE_TEXT), iter(MINOR_TEXT)
ks_pool, nt_pool, mn_pool = list(KEYSTONE_TEXT), list(NOTABLE_TEXT), list(MINOR_TEXT)
node_name = {}
ki = ni = mi = 0
for n in order:
    k = kind_of(nodes[n])
    if k == "keystone":
        node_name[n] = ks_pool[ki % len(ks_pool)][0]
        ki += 1
    elif k == "notable":
        node_name[n] = nt_pool[ni % len(nt_pool)][0]
        ni += 1
    elif k == "jewel":
        node_name[n] = "Jewel Socket"
    elif k == "mastery":
        node_name[n] = "Ember Mastery"
    else:
        node_name[n] = mn_pool[mi % len(mn_pool)][0]
        mi += 1

w("inline constexpr Node kNodes[] = {")
for n in order:
    x, y = to_scene(pts[n])
    nd = nodes[n]
    k = kind_of(nd)
    state = ("State::Allocated" if n in allocated else
             "State::CanAllocate" if n in frontier else "State::Unallocated")
    w('    {%.2ff, %.2ff, %d, %d, Kind::%s, %s, "%s"},' %
      (x, y, group_index[nd["group"]], nd.get("orbit", 0),
       k.capitalize(), state, esc(node_name[n])))
w("};")
w("inline constexpr int kNodeCount = (int)(sizeof(kNodes) / sizeof(kNodes[0]));")
w("")

w("inline constexpr Edge kEdges[] = {")
emitted = set()
for a in order:
    for b in sorted(sub[a]):
        key = tuple(sorted((index[a], index[b])))
        if key in emitted:
            continue
        emitted.add(key)
        na, nb = nodes[a], nodes[b]
        arc = (na["group"] == nb["group"] and na.get("orbit", 0) == nb.get("orbit", 0)
               and na.get("orbit", 0) > 0)
        w("    {%d, %d, %s, %s}," %
          (key[0], key[1], "true" if arc else "false",
           str(group_index[na["group"]]) if arc else "0xFF"))
w("};")
w("inline constexpr int kEdgeCount = (int)(sizeof(kEdges) / sizeof(kEdges[0]));")
w("")

w("inline constexpr Detail kDetails[] = {")
for name, stats, flavour in KEYSTONE_TEXT:
    padded = list(stats) + [None] * (3 - len(stats))
    w('    {"%s", "KEYSTONE", {%s}, "%s"},' %
      (esc(name),
       ", ".join('"%s"' % esc(s) if s else "nullptr" for s in padded),
       esc(flavour)))
for name, stats in NOTABLE_TEXT:
    padded = list(stats) + [None] * (3 - len(stats))
    w('    {"%s", "NOTABLE", {%s}, nullptr},' %
      (esc(name), ", ".join('"%s"' % esc(s) if s else "nullptr" for s in padded)))
w("};")
w("inline constexpr int kDetailCount = "
  "(int)(sizeof(kDetails) / sizeof(kDetails[0]));")
w("")
w("} // namespace compose_gallery::skill_tree_data")
w("")

OUT.write_text("\n".join(lines))
print("nodes", len(order), "edges", len(emitted), "groups", len(used_groups))
print("allocated run", len(allocated), "frontier", len(frontier))
print("scale %.5f  offset %.1f,%.1f" % (scale, offX, offY))
print("wrote", OUT)
