# Findings

## Font cache purge retains optical-kerning measurements

`FontContext::purgeAllCaches()` clears shaping, fallback, typeface and language
storage, but leaves `glyphProfiles` and `referenceGaps` populated. Its public
contract says that every context-owned cache is released, including for a
long-lived process whose typeface population changes. Clear both optical-
kerning maps during the full purge. A test should populate those measurements,
purge the context, assert that no optical measurements remain retained, and
verify that shaping the same text repopulates them with unchanged advances.
