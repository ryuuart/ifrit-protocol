"""Native motion values, shared live outputs, and retained animation descriptions.

Times are seconds, including Transition duration/delay and through() waypoints.
Output values survive Python wrapper collection while a native description or
binding still references them. The sketch ticker owns the scene clock.
"""

from _sigil.motion import *
