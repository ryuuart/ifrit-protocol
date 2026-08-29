#!/bin/sh
# Regenerates the committed Python schema modules from SpellCircle.fbs,
# for apps/python/SpellCircle. Run from anywhere; paths resolve relative
# to the repository root.
#
# Only the Python side is here. The C++ header is generated into the
# build tree by the SpellCircleSchema target and is not committed, so it
# needs nothing from this script. The Python package is installed and
# imported on its own — by TouchDesigner among others — with no CMake
# build in reach, which is why its modules stay committed and why
# regenerating them is a manual step to run after every schema edit.
set -eu

repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
schema="$repo_root/apps/spell-circle-canvas/src/spellcircle/shared/schema/SpellCircle.fbs"

# The Python generator writes a package, not just modules: alongside the
# per-table modules it emits an empty SpellCircle/__init__.py. That name
# is already taken by the hand-written public API, so generating straight
# into apps/python would silently empty it. Generate aside, then take
# only the modules the schema actually defines.
python_staging=$(mktemp -d)
trap 'rm -rf "$python_staging"' EXIT
flatc --python -o "$python_staging" "$schema"

for module in "$python_staging"/SpellCircle/*.py; do
    name=$(basename "$module")
    [ "$name" = "__init__.py" ] && continue
    cp "$module" "$repo_root/apps/python/SpellCircle/$name"
done

echo "Regenerated Python schema modules from $schema"
