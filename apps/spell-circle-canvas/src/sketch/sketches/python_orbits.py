"""Four periodic paths drawn from the scene clock with the native pen."""

from math import cos, sin, tau

from sigil.sketch import sketch

ORBITS = [
    ("IO", 88, 7, (243, 181, 117)),
    ("EUROPA", 142, 11, (132, 206, 194)),
    ("GANYMEDE", 198, 17, (130, 170, 226)),
    ("CALLISTO", 254, 23, (218, 155, 190)),
]


@sketch(size=(1100, 740), background="#111b27", capture_at=3.0)
class Orbits:
    def draw(self, pen):
        t = pen.millis() / 1000
        cx, cy = 374, 405
        pen.background("#111b27")
        pen.noStroke()
        pen.fill("#7e93a9")
        pen.textSize(12)
        pen.text("FIELD NOTES    /    001", 44, 44)
        pen.fill("#e8eef2")
        pen.textSize(36)
        pen.text("Periodic arrangements", 42, 88)
        pen.stroke("#2b3b4c")
        pen.strokeWeight(1)
        pen.line(44, 116, 1056, 116)
        pen.line(720, 158, 720, 650)

        pen.noFill()
        for i in range(72):
            angle = tau * i / 72
            outer = 274 if i % 6 else 281
            pen.stroke("#35485b" if i % 6 == 0 else "#233343")
            pen.line(
                cx + 269 * cos(angle),
                cy + 269 * sin(angle),
                cx + outer * cos(angle),
                cy + outer * sin(angle),
            )

        for i, (label, radius, period, color) in enumerate(ORBITS):
            angle = tau * (t / period + i * 0.19)
            pen.noFill()
            pen.stroke("#2b3b4b")
            pen.strokeWeight(1)
            pen.circle(cx, cy, radius * 2)
            for step in range(24):
                phase = angle - 0.045 * step
                pen.stroke(*color, 180 * (1 - step / 24))
                pen.strokeWeight(2)
                pen.arc(cx, cy, radius * 2, radius * 2, phase - 0.04, phase)
            x, y = cx + radius * cos(angle), cy + radius * sin(angle)
            pen.noStroke()
            pen.fill(*color, 24)
            pen.circle(x, y, 30)
            pen.fill(*color)
            pen.circle(x, y, 11)

            y = 260 + i * 92
            pen.circle(772, y - 5, 8)
            pen.textSize(15)
            pen.text(label, 795, y)
            pen.fill("#8297ac")
            pen.textSize(13)
            pen.text(f"{period:02d} second orbit", 795, y + 23)

        pen.noStroke()
        pen.fill("#f1d6ad")
        pen.circle(cx, cy, 20)
        pen.fill(241, 214, 173, 16)
        pen.circle(cx, cy, 42)
        pen.fill("#8095a9")
        pen.textSize(12)
        pen.text("FOUR RATES", 768, 185)
        pen.text("ONE SHARED CLOCK", 768, 205)
        pen.stroke("#2b3b4c")
        pen.strokeWeight(1)
        pen.line(44, 687, 1056, 687)
        pen.noStroke()
        pen.fill("#8297ac")
        pen.text("Circular paths, independent phases.", 44, 714)
        pen.text(f"T + {t:05.2f} s", 950, 714)
