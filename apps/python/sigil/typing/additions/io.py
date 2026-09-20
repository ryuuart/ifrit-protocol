"""Native resource access, live byte feeds, recording and data output.

Sketches use ``ctx.assets.hub()``: the host registers transports, advances
recordings and owns the session's feed leases. Standalone Python programs
construct ``Hub()`` and register the transports they need. A feed's receive
method never waits; decode its owned bytes on the calling Python thread.
"""
