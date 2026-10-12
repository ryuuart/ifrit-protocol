"""
me - this DAT.

frame - the current frame number.

An Execute DAT with Frame Start on runs onFrameStart once a frame. This
one reads the patch's cue — the levels from a CHOP, the colour from a
CHOP, the markers from a Table DAT — and sends it through a UDP Out DAT
to the cue_deck sketch. The sketch draws the cue, Grimoire publishes the
drawing over Syphon, and a Syphon Spout In TOP brings it back into this
patch.

The operators it reads, by name, beside this DAT:

    cue_levels   any CHOP; one channel per meter, named as the meter is
                 labelled, each from zero to one, first sample read
    cue_color    a Constant CHOP with channels r, g and b, each zero to one
    cue_markers  a Table DAT with a header row and columns name, beat
    cue_out      a UDP Out DAT: Network Address 127.0.0.1, Network Port 27025
    show_in      a Syphon Spout In TOP: Sender Name Cue Deck — not read
                 here; it is where the published drawing comes back

THREE ROUTES to the same page:

    JSON         send_json(), what onFrameStart calls: the cue as JSON
                 text, one datagram, to the sketch's door on port 27025.

    OSC          no script at all. Merge the CHOPs, name every channel by
                 the address it goes out as — /cue/level/KEY, /cue/color/r,
                 /cue/beat — with a Rename CHOP, and wire the merge into an
                 OSC Out CHOP: Network Address 127.0.0.1, Network Port
                 27026, one message per channel rather than one bundle. A
                 CHOP carries numbers only, so the show's name and the
                 markers stay off this route.

    FLATBUFFER   send_flatbuffer(), for when the cue has settled into
                 the sketch's schema: the cue built into one Cue with the
                 modules flatc generated from cue_deck.fbs, one datagram
                 to port 27027. Set cue_out's Network Port to 27027 and
                 call it from onFrameStart instead of send_json().

A CHOP Execute DAT watching cue_levels can call send_json() from
onValueChange instead, which sends only when a level moves.
"""

import json
import sys
from pathlib import Path

SHOW = "NIGHT GARDEN · ACT II"
TEMPO = 120.0  # beats a minute
BAR = 16.0  # the beats the sketch's ruler spans


def read_cue():
    """The patch's cue, in the shape the sketch reads."""
    levels, color, markers = op("cue_levels"), op("cue_color"), op("cue_markers")
    return {
        "show": SHOW,
        "channels": [
            {"name": channel.name, "level": float(channel[0])}
            for channel in levels.chans()
        ],
        "color": {name: float(color[name]) for name in ("r", "g", "b")},
        "markers": [
            {"name": row[0].val, "beat": float(row[1].val)}
            for row in markers.rows()[1:]
        ],
        "beat": absTime.seconds * TEMPO / 60.0 % BAR,
    }


def send_json():
    """This frame's cue as JSON text, one datagram."""
    op("cue_out").sendBytes(json.dumps(read_cue()).encode())


def onFrameStart(frame):
    send_json()
    return


# ── the FlatBuffer route, once the cue has settled ─────────────────────


def send_flatbuffer():
    """This frame's cue as one Cue FlatBuffer, one datagram."""
    op("cue_out").sendBytes(encode(read_cue()))


def encode(cue):
    """One Cue FlatBuffer. A builder writes back to front, so every string
    and every table a vector holds is made before the vector, and every
    vector before the table that holds it."""
    # THE GENERATED MODULES. flatc writes the CueDeck package beside the
    # sketch's schema, and they are read where they stand, so a schema
    # edited and regenerated is what the next frame sends. project.folder
    # is the folder this .toe is saved in, one level below the repository
    # root.
    sketch = Path(project.folder).parent / "apps" / "grimoire" / "sketches" / "cue_deck"
    if str(sketch) not in sys.path:
        sys.path.append(str(sketch))
    import flatbuffers
    from CueDeck import Channel, Cue, Marker
    from CueDeck.Color import CreateColor

    builder = flatbuffers.Builder(512)
    show = builder.CreateString(cue["show"])

    channels = []
    for channel in cue["channels"]:
        label = builder.CreateString(channel["name"])
        Channel.Start(builder)
        Channel.AddName(builder, label)
        Channel.AddLevel(builder, channel["level"])
        channels.append(Channel.End(builder))
    Cue.StartChannelsVector(builder, len(channels))
    for channel in reversed(channels):
        builder.PrependUOffsetTRelative(channel)
    channel_vector = builder.EndVector()

    markers = []
    for marker in cue["markers"]:
        label = builder.CreateString(marker["name"])
        Marker.Start(builder)
        Marker.AddName(builder, label)
        Marker.AddBeat(builder, marker["beat"])
        markers.append(Marker.End(builder))
    Cue.StartMarkersVector(builder, len(markers))
    for marker in reversed(markers):
        builder.PrependUOffsetTRelative(marker)
    marker_vector = builder.EndVector()

    color = cue["color"]
    Cue.Start(builder)
    Cue.AddShow(builder, show)
    Cue.AddChannels(builder, channel_vector)
    Cue.AddColor(builder, CreateColor(builder, color["r"], color["g"], color["b"]))
    Cue.AddMarkers(builder, marker_vector)
    Cue.AddBeat(builder, cue["beat"])
    builder.Finish(Cue.End(builder))
    return bytes(builder.Output())


def onStart():
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
