# Findings

## Column balancing is reported as unavailable

The typography parity table labels frame column balancing as not started.
Compose implements balanced frame runs through `Element::balanceChain`,
and its column component can balance the text above a spanning element.
The capability description should state those supported operations and
their requirement for a declared pixel depth. Existing frame and column
cases should continue to assert equal resolved depths, preserved text
continuation, and balancing above a spanner; documentation checks should
reject a capability entry that reports those operations as unavailable.

## Deferred component theme scope is described incorrectly

The SketchKit theme guidance groups deferred memo descriptions with custom
paint callbacks and says they run without an inherited scope. A memo
captures the environment where it is described, restores it around its
deferred call, and re-describes when the captured environment changes.
The guidance should distinguish memo descriptions from paint callbacks.
The environment regression should continue to assert that a memo reads
its captured theme after the provider scope ends, reuses equal props and
environment, and updates when the environment changes.
