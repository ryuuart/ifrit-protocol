"""The one way a test puts a message on a feed without opening a socket.

``inletOf(feed)`` hands back the feed's ``Inlet``: its ``deliver`` puts a
message on the feed as a transport would, naming a sender or stamped with a
recording's time, and ``fail`` says what went wrong. A reader of the feed has
neither; a sketch reads what a transport delivered.
"""
