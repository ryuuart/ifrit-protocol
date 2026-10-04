"""Study · Typography — a radio broadside printed from two plates.

TAGS: Studies/Cultural, Typography/Lettering, Typography/Effects, Materials/Shaders, Drawing/Generative, Drawing/Engraving, Runtime/Python

Shaped Cyrillic follows a transmission axis. A ruled steel tower and a
screened metal horn are authored native imagery, not archival photographs.
"""

from functools import lru_cache
from math import cos, pi, sin

from sigil.compose import Cache, PaintBox, box, text
from sigil.compose import pen as canvas
from sigil.draw import CLOSE, DEGREES
from sigil.material import Color, shader
from sigil.sketch import SketchContext, sketch
from sigil.weave import FrameOptions, Type

SIZE = (900, 1260)
PAPER = "#DFCC9E"
BLACK = "#222921"
RED = "#AC2D1D"
TITLE = "Arial Black, Arial, Helvetica Neue"
NARROW = "Arial Narrow, Avenir Next Condensed, Arial"
REGULAR = "Arial, Helvetica Neue"

PAPER_SOURCE = """
float hash(float2 p) { return fract(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float noise(float2 p) {
  float2 i=floor(p),f=fract(p); f=f*f*(3-2*f);
  return mix(mix(hash(i),hash(i+float2(1,0)),f.x),mix(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
}
half4 main(float2 p) {
  float2 uv=p/float2(900,1260);
  float edge=min(min(uv.x,1-uv.x),min(uv.y,1-uv.y));
  float stain=(1-smoothstep(0.0,0.075,edge))*(0.043+0.02*noise(p/64));
  float fibre=pow(noise(float2(p.x*1.9,p.y*.067)),7.0);
  float crease=exp(-abs(p.x-455.0-1.5*sin(p.y/79.0))/1.2)*.034;
  float pressure=(noise(p/103)-.5)*.024+(noise(p/22)-.5)*.009;
  float satin=.0025*sin(uTime*.4+uv.x*2);
  float3 c=base.rgb+pressure+satin-stain-fibre*.018-crease;
  return half4(c,1);
}
"""

INK_SOURCE = """
float hash(float2 p) { return fract(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float noise(float2 p) {
  float2 i=floor(p),f=fract(p); f=f*f*(3-2*f);
  return mix(mix(hash(i),hash(i+float2(1,0)),f.x),mix(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
}
half4 main(float2 p) {
  float press=noise(p/43)*.48+noise(p/11)*.22+.3;
  float fibre=pow(noise(float2(p.x*1.4,p.y*.12)),11.0);
  float a=pigment.a*clamp(.91+press*.09-fibre*loss,0,1);
  return half4(pigment.rgb*a,a);
}
"""

# A stock dot tile has one radius; this screen varies ink area with local
# metal lighting and changes to negative holes in deep shadow.
HORN_SOURCE = """
float hash(float2 p) { return fract(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float noise(float2 p) {
  float2 i=floor(p),f=fract(p); f=f*f*(3-2*f);
  return mix(mix(hash(i),hash(i+float2(1,0)),f.x),mix(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
}
half4 main(float2 p) {
  float2 v=p-float2(302,698);
  float angle=-.29;
  float2 q=float2(cos(angle)*v.x+sin(angle)*v.y,-sin(angle)*v.x+cos(angle)*v.y);
  float2 n=q/float2(225,163);
  float r=length(n);
  float cut=1-smoothstep(1.009,1.016,r);
  if(cut<=0) return half4(0);
  float3 lamp=normalize(float3(-.45,-.63,.62));
  float3 normal=normalize(float3(n.x*.74,n.y*.97,.48));
  float tone=.14+.76*max(0,dot(normal,lamp));
  float rim=abs(r-.955)/.058;
  if(r>.9) {
    float height=sqrt(max(0,1-rim*rim));
    float3 rolled=normalize(float3(n.x*rim,n.y*rim,height*.85));
    tone=.21+.75*max(0,dot(rolled,lamp));
  }
  float2 throat=(q-float2(-37,17))/float2(48,36);
  float tr=length(throat);
  tone*=smoothstep(.63,1.20,tr);
  tone+=.027*sin(r*310+atan(n.y,n.x)*.12)*smoothstep(.85,.73,r);
  tone-=.08*noise(q/39)+.02*noise(q/7);
  float highlight=exp(-pow((r-.895)/.009,2.0));
  tone+=highlight*.32;
  if(r>.97) tone=.045+.40*max(0,-n.x*.4-n.y*.8);
  if(tr<.74) tone=.015;
  float shadow=clamp(1.05-clamp(tone,0,1)*1.18,.025,.99);
  float2 screen=float2((p.x+p.y)*.7071,(-p.x+p.y)*.7071)/3.2;
  float dot=length(fract(screen)-.5);
  float pitch=3.2*max(uContentScale,.01);
  float aa=max(.034,.5/pitch);
  float coverage;
  if(shadow<=.5) {
    float radius=sqrt(shadow/3.14159);
    coverage=1-smoothstep(radius-aa,radius+aa,dot);
  } else {
    float hole=sqrt((1-shadow)/3.14159);
    float inverted=length(fract(screen+.5)-.5);
    coverage=smoothstep(hole-aa,hole+aa,inverted);
  }
  coverage=mix(shadow,coverage,smoothstep(1.8,3.6,pitch));
  float hatch=.018*pow(.5+.5*sin(q.x*.9+q.y*.34),15.0)*(1-tone);
  float3 c=mix(stock.rgb,pigment.rgb,clamp(coverage+hatch,0,1));
  return half4(c*cut,cut);
}
"""


@lru_cache(maxsize=24)
def ink(color, loss=0.025):
    return shader(INK_SOURCE, {"pigment": Color(color), "loss": loss})


def polygon(p, points, fill=None, edge=None, weight=0.8):
    if fill is None:
        p.noFill()
    else:
        p.fill(fill)
    if edge is None:
        p.noStroke()
    else:
        p.stroke(edge)
        p.strokeWeight(weight)
    p.beginShape()
    for x, y in points:
        p.vertex(x, y)
    p.endShape(CLOSE)


def tone(light, opacity=1):
    light = max(0, min(1, light))
    return tuple(
        (a + (b - a) * light) / 255 for a, b in zip((34, 41, 33), (223, 204, 158))
    ) + (opacity,)


def line(p, a, b, color, weight=0.8):
    p.stroke(color)
    p.strokeWeight(weight)
    p.line(*a, *b)


def structure(p):
    polygon(p, [(35, 294), (858, 129), (858, 149), (35, 314)], ink(BLACK))
    polygon(p, [(39, 324), (440, 243), (440, 274), (39, 355)], ink(BLACK))
    polygon(p, [(565, 208), (858, 149), (858, 470), (565, 529)], ink(BLACK))
    polygon(p, [(383, 566), (858, 351), (858, 718), (383, 759)], ink(RED))
    polygon(p, [(40, 1025), (858, 1025), (858, 1156), (40, 1156)], ink(RED))
    p.noFill()
    p.stroke(ink(BLACK))
    p.strokeWeight(1.1)
    for i in range(6):
        p.ellipse(667, 539, 170 + i * 22, 98 + i * 16)
    polygon(p, [(565, 493), (858, 437), (858, 470), (565, 528)], ink(BLACK))
    for i in range(4):
        polygon(
            p,
            [
                (40 + i * 23, 367),
                (53 + i * 23, 364),
                (111 + i * 23, 410),
                (99 + i * 23, 413),
            ],
            ink(BLACK),
        )


def tower(p):
    p.push()
    p.clip(lambda: p.rect(40, 95, 818, 907))
    # Every generator joins the rotated end rings of one hyperboloid.
    # Beam highlights share one oblique light, with darker back members.
    radii = (135, 108, 86, 68, 50, 33, 18)

    def point(t, radius, angle):
        cx, cy = 158 + t * 503, 992 - t * 728
        cross, depth = cos(angle) * radius, sin(angle) * radius * 0.24
        return (
            cx + cross * 0.821 + depth * 0.571,
            cy + cross * 0.571 - depth * 0.821,
            sin(angle),
        )

    p.noFill()
    p.stroke(tone(0.86))
    p.strokeWeight(8)
    for side in range(2):
        for s in range(6):
            a, b = (
                point(s / 6, radii[s], 0 if side else pi),
                point((s + 1) / 6, radii[s + 1], 0 if side else pi),
            )
            p.line(a[0] + 2, a[1] + 1, b[0] + 2, b[1] + 1)
    for back in range(2):
        for s in range(6):
            for hand in range(2):
                for i in range(24):
                    aa = (i + 0.11) * 2 * pi / 24
                    bb = aa + (-0.64 if hand else 0.64)
                    a, b = (
                        point(s / 6, radii[s], aa),
                        point((s + 1) / 6, radii[s + 1], bb),
                    )
                    if ((a[2] + b[2]) * 0.5 >= 0) != bool(back):
                        continue
                    line(
                        p,
                        a[:2],
                        b[:2],
                        tone(0.06 if back else 0.40),
                        2.1 if back else 1.65,
                    )
                    line(
                        p,
                        (a[0] - 0.47, a[1] - 0.22),
                        (b[0] - 0.47, b[1] - 0.22),
                        tone(0.48 if back else 0.71),
                        0.42,
                    )
                    p.noStroke()
                    p.fill(tone(0.13 if back else 0.52))
                    for j in range(1, 6):
                        t = j / 6
                        p.circle(
                            a[0] + (b[0] - a[0]) * t,
                            a[1] + (b[1] - a[1]) * t,
                            2.6 if back else 1.8,
                        )
    for s in range(7):
        ring = [point(s / 6, radii[s], i * 2 * pi / 96)[:2] for i in range(97)]
        polygon(p, ring, None, tone(0.18), 5 - s * 0.45)
        polygon(p, ring, None, tone(0.67), 0.70)
        for i in range(24):
            q = point(s / 6, radii[s], i * 2 * pi / 24)
            p.noStroke()
            p.fill(tone(0.08))
            p.circle(q[0], q[1], 3.2 - s * 0.23)
            p.fill(tone(0.70))
            p.circle(q[0] - 0.3, q[1] - 0.3, 1.3)
    line(p, (164, 993), (664, 265), tone(0.10), 2.2)
    line(p, (174, 998), (673, 270), tone(0.10), 2.2)
    for i in range(91):
        t = i / 90
        line(
            p,
            (164 + 500 * t, 993 - 728 * t),
            (174 + 499 * t, 998 - 728 * t),
            tone(0.63),
            0.70,
        )

    p.pop()


def horn_mount(p):
    polygon(p, [(49, 738), (195, 671), (241, 706), (80, 801)], ink(BLACK))
    polygon(p, [(51, 737), (67, 739), (195, 680), (195, 671)], tone(0.40))
    polygon(p, [(57, 741), (82, 752), (65, 945), (31, 942)], ink(BLACK))
    polygon(p, [(57, 741), (64, 744), (48, 939), (39, 940)], tone(0.57))
    polygon(p, [(27, 937), (67, 944), (106, 986), (20, 974)], tone(0.14))
    polygon(p, [(28, 935), (65, 941), (72, 948), (31, 940)], tone(0.70))
    for i in range(4):
        p.noStroke()
        p.fill(tone(0.73))
        p.circle(42 + i * 3, 771 + i * 36, 4)
        p.fill(tone(0.04))
        p.circle(42 + i * 3, 771 + i * 36, 1.5)


def streets(p):
    # Roof planes, brick courses, window reveals and tram wires share a
    # receding diagonal. These are fabricated buildings, not city records.
    p.push()
    p.translate(448, 822)
    p.angleMode(DEGREES)
    p.rotate(-11)
    polygon(p, [(0, 150), (379, 41), (379, 103), (0, 183)], tone(0.19))
    heights = (157, 99, 180, 137, 213, 157, 126, 93)
    for i in reversed(range(8)):
        x, y, w, h = i * 43, 121 - i * 7.8, 39, heights[i] * (1 - i * 0.047)
        polygon(
            p,
            [(x, y), (x, y - h), (x + w, y - h - 10), (x + w, y - 10)],
            tone(0.13 + i * 0.032),
        )
        polygon(
            p,
            [
                (x, y - h),
                (x + 12, y - h - 7),
                (x + w + 12, y - h - 15),
                (x + w, y - h - 10),
            ],
            tone(0.58 + i * 0.01),
        )
        polygon(
            p,
            [
                (x + w, y - h - 10),
                (x + w + 12, y - h - 15),
                (x + w + 12, y - 17),
                (x + w, y - 10),
            ],
            tone(0.33),
        )
        yy = y - h + 7
        while yy < y - 5:
            line(p, (x + 1, yy), (x + w - 1, yy - 9), tone(0.62), 0.6)
            yy += 7.5
        for row in range(int(h / 20) - 1):
            for col in range(3):
                xx, yy = x + 5 + col * 11, y - h + 14 + row * 20 - col * 2.5
                p.noStroke()
                p.fill(tone(0.035))
                p.rect(xx, yy, 6.2, 10.5)
                p.fill(tone(0.66))
                p.rect(xx + 0.8, yy + 0.9, 2.2, 8.8)
                line(p, (xx - 1, yy + 11.5), (xx + 7, yy + 10.1), tone(0.57), 0.5)
        line(p, (x + 8, y - h), (x + 8, y - h - 19), tone(0.06), 1.1)
        line(p, (x + 3, y - h - 15), (x + 31, y - h - 23), tone(0.06), 1.1)
    p.noFill()
    p.stroke(tone(0.1))
    p.strokeWeight(0.65)
    for i in range(4):
        p.bezier(
            -5, 13 + i * 15, 121, -13 + i * 14, 264, -60 + i * 13, 408, -84 + i * 12
        )
    p.pop()


def legend(p):
    polygon(p, [(439, 958), (858, 878), (858, 917), (439, 997)], ink(BLACK))


def impressions(p):
    p.noFill()
    p.stroke(tone(0.3))
    p.strokeWeight(0.6)
    for x, y in ((23, 23), (877, 23), (23, 1237), (877, 1237)):
        p.circle(x, y, 11)
        p.line(x - 9, y, x + 9, y)
        p.line(x, y - 9, x, y + 9)
    p.line(40, 1004, 858, 1004)
    p.line(40, 1172, 858, 1172)
    p.stroke(tone(0.7, 0.18))
    for i in range(14):
        y = 90 + i * 79
        p.bezier(451, y, 453, y + 12, 460, y + 26, 455, y + 45)
    p.stroke(tone(0.68, 0.38))
    p.strokeWeight(0.45)
    for i in range(9):
        p.line(855 - i * 0.41, 43, 855 - i * 0.43, 984)
    p.noStroke()
    p.fill(tone(0.40, 0.35))
    p.rect(41, 1197, 189, 1)
    for i in range(8):
        p.fill(ink(RED if i % 2 else BLACK))
        p.rect(692 + i * 20, 1192, 16, 8)


def words(
    value, x, y, width, size, color=BLACK, rotation=0, track=0, condense=1, family=TITLE
):
    return (
        text(value)
        .fontFamily(family)
        .font(
            Type(
                size=size,
                weight=900 if family == TITLE else 700,
                track=track,
                condense=condense,
                language="ru",
            )
        )
        .ink(ink(color), PaintBox.Canvas)
        .absolute()
        .left(x)
        .top(y)
        .width(width)
        .height(size * 1.35)
        .textFirstBaseline(FrameOptions.FirstBaseline.CapHeight)
        .maxTextLines(1)
        .transformOrigin("0%", "0%")
        .rotate(rotation)
    )


@sketch(size=SIZE, background=PAPER, capture_at=4)
class ConstructivistRadio:
    def setup(self, ctx: SketchContext):
        letters = [
            words("РАДИО", 37, 142, 850, 182, BLACK, -11.4, -8, 0.95),
            words(
                "ВОЗДУХ СТАНОВИТСЯ ГОЛОСОМ",
                49,
                47,
                817,
                16,
                BLACK,
                track=2,
                family=NARROW,
            ),
            words(
                "THE AIR BECOMES A VOICE",
                49,
                75,
                817,
                12,
                RED,
                track=2.6,
                family=REGULAR,
            ),
            words("02", 722, 63, 145, 71, RED, track=-1),
            words("ЭФИР", 599, 278, 264, 52, PAPER, -11.4, 1, 0.88),
            words("ЛИТЕРАТУРА", 603, 342, 264, 19, PAPER, -11.4, 1.4, family=NARROW),
            words("МУЗЫКА", 610, 376, 230, 19, PAPER, -11.4, 1.4, family=NARROW),
            words("КИНО", 615, 411, 219, 19, PAPER, -11.4, 1.4, family=NARROW),
            words("СЛУШАИ\u0306", 446, 567, 446, 73, PAPER, -24, 0, 0.89),
            words("ГОРОД", 475, 642, 433, 109, PAPER, -24, -3, 0.88),
            words("ГОВОРИТ", 535, 719, 326, 35, BLACK, -11.4, 1, 0.9),
            words(
                "VOICE / WAVE / CITY",
                55,
                359,
                480,
                16,
                BLACK,
                -11.4,
                1.4,
                family=NARROW,
            ),
            words("АНТЕННА", 35, 895, 270, 18, BLACK, -55.4, 3, family=NARROW),
            words("ЗВУК ДЛЯ ВСЕХ", 451, 962, 402, 24, PAPER, -10.8, 1.1, 0.88),
            words("НАСТРОЙСЯ", 60, 1043, 797, 62, PAPER, track=-1, condense=0.94),
            words("НА ВОЛНУ", 530, 1101, 326, 34, PAPER, track=1.2, condense=0.88),
            words("06:00 — 24:00", 61, 1120, 386, 26, PAPER, track=1.5, family=NARROW),
            words(
                "A TRANSMISSION IN TWO INKS",
                41,
                1183,
                589,
                12,
                BLACK,
                track=1.6,
                family=NARROW,
            ),
            words(
                "СЛОВА / ЗВУК / ПРОСТРАНСТВО / ДВИЖЕНИЕ",
                41,
                1216,
                817,
                11,
                BLACK,
                track=1.6,
                family=REGULAR,
            ),
        ]
        ctx.render(
            box()
            .width(900)
            .height(1260)
            .children(
                box()
                .absolute()
                .inset(0)
                .fill(shader(PAPER_SOURCE, {"base": Color(PAPER)})),
                canvas("radio.structure", structure, Cache.Picture)
                .absolute()
                .inset(0)
                .translateX(0.65)
                .translateY(0.35),
                canvas("radio.tower", tower, Cache.Texture).absolute().inset(0),
                canvas("radio.mount", horn_mount, Cache.Picture).absolute().inset(0),
                box()
                .absolute()
                .inset(0)
                .fill(
                    shader(
                        HORN_SOURCE, {"pigment": Color(BLACK), "stock": Color(PAPER)}
                    )
                ),
                canvas("radio.street", streets, Cache.Texture).absolute().inset(0),
                canvas("radio.legend", legend, Cache.Picture).absolute().inset(0),
                *letters,
                canvas("radio.impressions", impressions, Cache.Picture)
                .absolute()
                .inset(0),
            )
        )
