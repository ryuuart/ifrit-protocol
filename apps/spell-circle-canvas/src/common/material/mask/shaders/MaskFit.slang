float shapeS(float raw) {
  float t = clamp((raw - low) / max(high - low, 1e-5), 0.0, 1.0);
  return inverted > 0.5 ? 1.0 - t : t;
}
