"""WHAT A DOCUMENTED NAME LOOKS LIKE, and which of them cannot be probed.

Four spellings are extracted from a document, and each is a pattern here:
a qualified name, a designated initialiser, the bullet that opens with a
header path (which is what makes the bare names inside it resolvable),
and the bare name itself.

Beside them stand the tables of what no probe form can reach.  Every
entry states WHY that name cannot resolve; an entry with no reason is a
hole in the guard, and a reason is what the coverage report prints so
the gap stays visible rather than folded into a count.
"""

import re

ID = r"[A-Za-z_][A-Za-z0-9_]*"
QUAL = re.compile(r"\b(" + ID + r"(?:::" + ID + r")+)")
# `Type{.field = …` / `Type{.field,` — a designated initialiser.  It needs
# its own pattern because it names no member in qualified form, so QUAL
# never sees it.
DESIG = re.compile(r"\b(" + ID + r"(?:::" + ID + r")*)\s*\{\s*\.(" + ID + r")")
DESIG_MORE = re.compile(r"[,{]\s*\.(" + ID + r")\s*(?:=|,|\})")
# A bullet that OPENS with one or more backticked header paths is an index
# of those headers: `- `core/Paint.h` — the paint values: …`.  Everything
# backticked in it is claimed to be a name those headers carry, which is
# what makes bare names resolvable here and nowhere else.
LISTING_OPENER = re.compile(
    r"^\s*[-*]\s+(`[A-Za-z0-9_./]+\.h`"
    r"(?:\s*(?:,|and|/)\s*`[A-Za-z0-9_./]+\.h`)*)"
)
HEADER_PATH = re.compile(r"`([A-Za-z0-9_./]+\.h)`")
BARE_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")

# Namespace names this generator cannot scrape, because the headers that
# declare them are not on the include path it is given.  Every other
# namespace comes from the scanned headers themselves.
#
# A name whose components are ALL namespaces has nothing to probe and is
# passed over; a missing entry here instead makes the first unrecognised
# component look like a type, and the name is reported unresolved.
NS_EXTERNAL = {
    # The standard library, its sub-namespaces the documents spell, and the
    # animation library compose exposes in its own signatures.  `ch` is the
    # short alias, which the generated translation unit declares to match.
    # Nested inline namespaces count: a document writing `using namespace
    # std::chrono_literals` names one, and a using-DECLARATION probe on a
    # namespace is ill-formed, so it has to be recognised here.
    "std",
    "chrono",
    "chrono_literals",
    "literals",
    "ranges",
    "views",
    "choreograph",
    "ch",
    # The vector maths every params struct is written in, exposed in this
    # tree's own signatures.
    "glm",
    # Boost.Asio's executor vocabulary is declared in dependency .hpp
    # headers, outside the library .h roots this scanner reads.
    "boost",
    "asio",
    # Skia's actual namespaces, which are Sk-prefixed like its types and
    # would otherwise be probed as if they were classes.
    "SkSurfaces",
    "SkShaders",
    "SkImages",
    "SkPathEffects",
    # SigilMaterial's root and its feature namespaces, whose include paths
    # are deliberately NOT all given: scanning the core would put every
    # type it declares into the candidate set a member probe ORs over, and
    # one of those (`ProgramCache::Key`) is a PRIVATE nested type, which
    # this scanner cannot see and which is a hard error the moment it is
    # named.  Only the Skia feature's headers are scanned, for the paint
    # value the documents name members of; the rest are listed here so a
    # document naming one bare is passed over rather than probed as a
    # type.
    "material",
    "sdf",
    "pattern",
    "field",
}

# Skia's static-factory aggregates that READ like namespaces but are CLASSES
# (`class SK_API SkImageFilters { static … }`).  This table is how such a
# name gets probed at all, not a way of skipping it: a namespace-scope
# using-declaration cannot name a class member and is ill-formed if it tries,
# while a DERIVED-CLASS using-declaration names one uniformly, overload sets
# included — the same one-spelling rule the namespace probe follows, one
# scope over.  Each maps to the Skia header that declares it, included only
# when a probe needs it.
EXTERNAL_CLASSES = {
    "SkImageFilters": "include/effects/SkImageFilters.h",
    "SkColorFilters": "include/core/SkColorFilter.h",
    "SkGradientShader": "include/effects/SkGradientShader.h",
    "SkFontMgr": "include/core/SkFontMgr.h",
}

# Whole spellings a document names on purpose that no header resolves —
# a symbol owned by a library this generator does not scan, or a name a
# document writes in order to say it does NOT exist.  Keyed by the exact
# qualified name, never by a component, so an exclusion can never widen
# silently to a sibling that SHOULD be probed.  Each entry states why that
# name cannot resolve; an entry with no such reason is a hole in the guard.
EXCLUDED_SPELLED = {}

# Single components that make any qualified name containing them
# unprobeable: an example's own host type, a placeholder standing in for a
# type the reader supplies, a template parameter recited from a header.
# Matched per component, which is why these spellings must be distinctive —
# a common word here silently exempts every name that contains it.
EXCLUDED = {}

# (type, member) pairs no probe form can SEE, as distinct from members that
# do not exist: overloaded member functions of types outside the scanned
# headers.  No `requires` spelling can name an overload set, and the header
# index below only covers headers this generator scans, so neither route
# reaches them.
UNPROBEABLE_MEMBERS = {}

# (type, member) pairs where the type is real, is scanned, and genuinely has
# no such member — a spelling a document writes to record that it is gone.
#
# NOTHING here may name a WRONG spelling of something that still exists in
# another form.  Such an entry disarms the guard for exactly the mistake it
# exists to catch: `PathFormat{.paint = …}` is wrong because `paint` is a
# member FUNCTION, and an entry for ("PathFormat", "paint") would turn that
# defect into a pass.  Prose that warns against a spelling should write it
# unqualified (`paint`), which is not a probed form and needs no entry at
# all.
EXCLUDED_MEMBERS = {}

# A dependency's OVERLOAD SET reached through a type this generator does
# not index: no `requires` spelling can name one, and the index path cannot
# answer either, since the index is built from the scanned headers alone.
EXCLUDED_SPELLED.update(
    {
        "SkRuntimeEffect::MakeForShader": "Skia's own overloaded static factory: no requires-spelling can "
        "name an overload set, and this generator indexes only the "
        "libraries whose include roots it is given",
    }
)


def resolve_type(name, types):
    """Fully qualified candidates for a type name as a document spells it."""
    if name in types:
        return ["::" + q for q in types[name]]
    if name.startswith("Sk") or name.startswith("Gr"):
        return ["::" + name]  # Skia lives at global scope
    return []
