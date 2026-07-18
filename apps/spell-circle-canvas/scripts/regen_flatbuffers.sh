#!/bin/sh
# Regenerates the committed FlatBuffers artifacts from SpellCircle.fbs —
# the C++ header consumed by SpellCircleNetwork/SpellCircleModels and the
# Python schema modules used by apps/python/SpellCircle. Run from anywhere;
# paths resolve relative to the repository root. Regeneration is manual on
# purpose (the generated header is committed), so run this after every
# schema edit and commit the results.
set -eu

repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
schema="$repo_root/apps/spell-circle-canvas/src/spellcircle/shared/schema/SpellCircle.fbs"

flatc --cpp -o "$repo_root/apps/spell-circle-canvas/src/spellcircle/shared/schema/include" "$schema"
flatc --python -o "$repo_root/apps/python" "$schema"

echo "Regenerated FlatBuffers artifacts from $schema"
