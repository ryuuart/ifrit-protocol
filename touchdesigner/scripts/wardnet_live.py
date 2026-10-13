"""
me - this DAT.

frame - the current frame number.

An Execute DAT with Frame Start on runs onFrameStart once a frame. This
one watches a CHOP and a Table DAT and sends what changed in them, as
JSON text through a UDP Out DAT, to the wardnet_live sketch: the load
from the CHOP whenever it moves, and every row added to the table once.
send_alert() and send_card() are there for a button's Panel Execute DAT
to call.

The operators it reads, by name, beside this DAT:

    wardnet_load   any CHOP with a channel named load, zero to one,
                   first sample read
    wardnet_rows   a Table DAT with a header row and columns tag, body;
                   a row appended to it is sent once, on the next frame
    wardnet_out    a UDP Out DAT: Network Address 127.0.0.1,
                   Network Port 27030

Every message is one datagram of JSON text whose "kind" names what it
changes, exactly as the sketch's sender.py and its browser page send:

    {"kind": "meter", "load": 0.7}
    {"kind": "row", "tag": "WARN", "body": "relay/05 jitter"}
    {"kind": "alert", "text": "BREACH · CHANNEL 9", "level": "breach"}
    {"kind": "card", "title": "RELAY JITTER", "value": "41 ms", "note": "relay/05"}
"""

import json

LOAD_STEP = 0.01  # how far the load moves before it is worth a message


def send(message):
    """One message as JSON text, one datagram."""
    op("wardnet_out").sendBytes(json.dumps(message).encode())


def send_load():
    """The load, when it has moved since the last one sent."""
    load = float(op("wardnet_load")["load"])
    if abs(load - me.fetch("load_sent", -1.0)) >= LOAD_STEP:
        me.store("load_sent", load)
        send({"kind": "meter", "load": round(load, 3)})


def send_new_rows():
    """Every row appended to the table since the last frame, in order."""
    rows = op("wardnet_rows").rows()[1:]
    sent = me.fetch("rows_sent", 0)
    for row in rows[sent:]:
        send({"kind": "row", "tag": row[0].val.upper(), "body": row[1].val})
    # A table emptied and filled again is read from its first row.
    me.store("rows_sent", len(rows))


def send_alert(text, level="breach"):
    send({"kind": "alert", "text": text, "level": level})


def send_card(title, value, note="", level="clear"):
    send({"kind": "card", "title": title, "value": value, "note": note, "level": level})


def onFrameStart(frame):
    send_load()
    send_new_rows()
    return


def onStart():
    # Nothing has been sent when the project starts.
    me.store("rows_sent", 0)
    me.store("load_sent", -1.0)
    return


def onCreate():
    return


def onExit():
    return


def onFrameEnd(frame):
    return


def onPlayStateChange(state):
    return


def onDeviceChange():
    return


def onProjectPreSave():
    return


def onProjectPostSave():
    return
