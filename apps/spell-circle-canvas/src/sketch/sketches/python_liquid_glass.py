"""Liquid glass fields and Bezier filaments share one Python model."""

# TAGS: Drawing/Generative, Materials/Shaders

from math import cos, hypot, sin

from sigil.draw import ADD, BLEND, CANVAS, ROUND, Pen
from sigil.material import skia
from sigil.sketch import SketchContext, sketch

Paint = skia.Paint

LINE_FIELD = r"""
      uniform float2 uResolution;
      uniform float uTime;
      half4 main(float2 xy) {
        float side = max(min(uResolution.x, uResolution.y), 1.0);
        float2 uv = (xy - uResolution * 0.5) / side;
        float radius = length(uv);
        float angle = atan(uv.y, uv.x);
        float warpX = uv.x + sin(uv.y * 8.0 - uTime * 0.72) * 0.075;
        float warpY = uv.y + sin(uv.x * 7.0 + uTime * 0.58) * 0.085;
        float a = sin(warpX * 43.0 + sin(warpY * 12.0) * 3.8);
        float b = sin(warpY * 37.0 + sin(warpX * 10.0) * 4.6);
        float c = sin(radius * 70.0 - angle * 5.0 - uTime * 1.25);
        float lineA = 1.0 - smoothstep(0.030, 0.095, abs(a));
        float lineB = 1.0 - smoothstep(0.026, 0.088, abs(b));
        float lineC = 1.0 - smoothstep(0.020, 0.072, abs(c));
        float3 colour = float3(0.004, 0.010, 0.032);
        colour += float3(0.02, 0.90, 1.18) * lineA;
        colour += float3(0.92, 0.08, 1.28) * lineB;
        colour += float3(1.34, 0.42, 0.04) * lineC * 0.72;
        float vignette = 1.0 - 0.62 * smoothstep(0.26, 0.78, radius);
        return half4(half3(colour * vignette), 1.0);
      }
    
"""

GLASS = r"""
      uniform shader uSource;
      uniform float4 uBall0;
      uniform float4 uBall1;
      uniform float4 uBall2;
      uniform float4 uBall3;
      uniform float4 uBall4;
      uniform float4 uBall5;
      uniform float4 uBall6;
      uniform float4 uBall7;
      uniform float uThreshold;
      uniform float uStrength;
      uniform float2 uResolution;
      uniform float uTime;

      float3 sampleBall(float2 xy, float4 ball) {
        float2 delta = xy - ball.xy;
        float squareDistance = max(dot(delta, delta), 4.0);
        float weight = ball.z * ball.z / squareDistance;
        return float3(weight, delta * weight / squareDistance);
      }

      half4 main(float2 xy) {
        float3 s0 = sampleBall(xy, uBall0);
        float3 s1 = sampleBall(xy, uBall1);
        float3 s2 = sampleBall(xy, uBall2);
        float3 s3 = sampleBall(xy, uBall3);
        float3 s4 = sampleBall(xy, uBall4);
        float3 s5 = sampleBall(xy, uBall5);
        float3 s6 = sampleBall(xy, uBall6);
        float3 s7 = sampleBall(xy, uBall7);
        float3 sum = s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7;
        float field = sum.x;
        float2 outward = normalize(sum.yz + float2(0.0001));
        float alpha = smoothstep(uThreshold * 0.86, uThreshold * 1.02,
                                 field);
        float depth = smoothstep(uThreshold, uThreshold * 4.2, field);
        float shell = 1.0 - smoothstep(uThreshold * 0.98,
                                       uThreshold * 1.34, field);
        float shimmer = 0.95 + 0.07 * sin(uTime * 1.7 + field * 0.82);
        float2 bend = -outward * uStrength * (0.20 + depth * 0.80) * shimmer;
        half4 redSample = uSource.eval(xy + bend * 1.075);
        half4 greenSample = uSource.eval(xy + bend);
        half4 blueSample = uSource.eval(xy + bend * 0.925);

        float3 normal = normalize(float3(outward * (0.80 - depth * 0.24),
                                         0.60 + depth * 0.40));
        float key = pow(max(dot(normal,
                                normalize(float3(-0.52, -0.62, 0.58))), 0.0),
                        42.0);
        float rim = pow(max(dot(normal,
                                normalize(float3(0.70, 0.10, 0.70))), 0.0),
                        68.0);
        float caustic = pow(max(sin(field * 11.0 - uTime * 1.8), 0.0), 18.0);
        float3 colour = float3(redSample.r, greenSample.g, blueSample.b);
        colour *= 1.12 + depth * 0.32;
        colour += float3(0.22, 0.90, 1.36) * shell * 1.34;
        colour += float3(2.20, 1.82, 0.96) * key * 1.82;
        colour += float3(1.02, 0.46, 1.72) * rim * 1.36;
        colour += float3(0.30, 1.08, 1.34) * caustic * depth * 0.42;
        return half4(half3(colour * alpha), half(alpha));
      }
    
"""

FILAMENT = r"""
      uniform float2 uResolution;
      uniform float uTime;
      half4 main(float2 xy) {
        float2 uv = xy / max(uResolution, float2(1.0));
        float pulse = 0.5 + 0.5 * sin((uv.x * 1.3 + uv.y) * 19.0 -
                                      uTime * 3.2);
        float3 cyan = float3(0.16, 1.18, 1.32);
        float3 pearl = float3(1.40, 1.32, 1.18);
        float3 magenta = float3(1.18, 0.28, 1.20);
        float3 colour = mix(cyan, magenta, uv.y * 0.72 + pulse * 0.18);
        colour = mix(colour, pearl, pow(pulse, 7.0) * 0.82);
        return half4(half3(colour), 0.86);
      }
    
"""


@sketch(size=(720, 720), background="#02050e", capture_at=0.05)
class LiquidGlass:
    def setup(self, ctx: SketchContext) -> None:
        self.source = Paint.sksl(LINE_FIELD).quantizeTime(30)
        self.filament = Paint.sksl(FILAMENT).quantizeTime(30)
        self.glass = (
            Paint.sksl(GLASS, {"uThreshold": 1.16, "uStrength": 42})
            .slot("uSource", self.source)
            .quantizeTime(30)
        )
        self.links = [
            (0, 1),
            (0, 2),
            (0, 3),
            (1, 4),
            (1, 5),
            (0, 6),
            (1, 6),
            (0, 7),
            (1, 7),
            (2, 6),
            (4, 6),
            (3, 7),
            (5, 7),
        ]

    def lobes(self, t):
        return [
            (302 + sin(t * 0.47) * 22, 352 + cos(t * 0.54) * 17, 112),
            (418 + cos(t * 0.43) * 24, 360 + sin(t * 0.51) * 20, 108),
            (132 + sin(t * 0.82) * 88, 224 + cos(t * 0.91) * 42, 88),
            (142 + cos(t * 0.68 + 0.4) * 82, 514 + sin(t * 0.77) * 48, 92),
            (584 - sin(t * 0.71 + 2) * 86, 220 + sin(t * 0.88 + 1.2) * 48, 90),
            (578 - cos(t * 0.73 + 1.1) * 84, 520 + cos(t * 0.84) * 42, 96),
            (356 + sin(t * 0.66 + 1.7) * 74, 102 + cos(t * 0.63 + 0.8) * 88, 82),
            (366 + cos(t * 0.61 + 2.5) * 70, 610 - cos(t * 0.63 + 0.8) * 78, 88),
        ]

    def tendrils(self, pen, balls, t, offset):
        for i, (a, b) in enumerate(self.links):
            ax, ay, _ = balls[a]
            bx, by, _ = balls[b]
            dx, dy = bx - ax, by - ay
            length = max(hypot(dx, dy), 1)
            nx, ny = -dy / length, dx / length
            curl = sin(t * (0.74 + i * 0.017) + i * 1.71) * (66 + (i * 17) % 52)
            ripple = cos(t * 1.13 + i * 0.83) * 38
            first, second = curl + offset, -curl * 0.58 + ripple + offset
            pen.bezier(
                ax,
                ay,
                ax + dx * 0.28 + nx * first,
                ay + dy * 0.28 + ny * first,
                ax + dx * 0.72 + nx * second,
                ay + dy * 0.72 + ny * second,
                bx,
                by,
            )

    def draw(self, pen: Pen) -> None:
        t = pen.millis() / 1000
        balls = self.lobes(t)
        glass = self.glass.copy()
        for i, (x, y, radius) in enumerate(balls):
            glass.uniform(f"uBall{i}", (x, y, radius, 0))
        pen.background(self.source)
        pen.blendMode(BLEND)
        pen.noStroke()
        pen.fill(glass, CANVAS)
        pen.rect(0, 0, pen.width, pen.height)
        pen.noFill()
        pen.strokeCap(ROUND)
        pen.strokeJoin(ROUND)
        pen.blendMode(ADD)
        pen.stroke(30, 205, 255, 30)
        pen.strokeWeight(5)
        self.tendrils(pen, balls, t, 0)
        pen.stroke(self.filament, CANVAS)
        pen.strokeWeight(1.35)
        for offset in (-7, 0, 7):
            self.tendrils(pen, balls, t, offset)
        pen.blendMode(BLEND)
