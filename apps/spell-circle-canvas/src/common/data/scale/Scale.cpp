#include <sigildata/scale/Scale.h>

#include <array>
#include <limits>

namespace sigil::data {

namespace {

/** The three factors a readable step is a multiple of a power of ten by,
 *  and the powers of ten themselves. A step is chosen by comparing the
 *  ideal spacing against the GEOMETRIC midpoints between those factors —
 *  sqrt(2), sqrt(10) and sqrt(50) — because the choice is between
 *  ratios, not between differences: 3 is as far above 2 as it is below
 *  5 only when the distance is measured multiplicatively. */
constexpr double kToTwo = 1.4142135623730951;   // sqrt(2)
constexpr double kToFive = 3.1622776601683795;  // sqrt(10)
constexpr double kToTen = 7.0710678118654755;   // sqrt(50)

/** A readable step, in the form the tick loop needs it.
 *
 *  A step below one is carried as the whole number it divides by rather
 *  than as the fraction it is, because a tenth cannot be written exactly
 *  in binary and five additions of it do not land on a half. Dividing
 *  the index instead puts every tick on the value a reader wrote. */
struct Step {
  double value = 0.0;      // the step itself, always positive
  double divisor = 0.0;    // non-zero when the step is 1 / divisor
  bool valid = false;
};

Step readableStep(double low, double high, int count) {
  if (count <= 0 || !(high > low)) return {};
  const double ideal = (high - low) / count;
  if (!std::isfinite(ideal) || ideal <= 0) return {};
  const double power = std::floor(std::log10(ideal));
  const double error = ideal / std::pow(10.0, power);
  const double factor = error >= kToTen    ? 10.0
                        : error >= kToFive ? 5.0
                        : error >= kToTwo  ? 2.0
                                           : 1.0;
  if (power >= 0) return {factor * std::pow(10.0, power), 0.0, true};
  return {factor / std::pow(10.0, -power), std::pow(10.0, -power) / factor,
          true};
}

/** Every multiple of @p step inside [low, high], ascending. */
std::vector<double> multiples(double low, double high, const Step& step) {
  std::vector<double> values;
  if (!step.valid) return values;
  if (step.divisor != 0.0) {
    const double first = std::ceil(low * step.divisor);
    const double last = std::floor(high * step.divisor);
    if (!(last >= first)) return values;
    values.reserve(static_cast<size_t>(last - first) + 1);
    for (double i = first; i <= last; i += 1.0)
      values.push_back(i / step.divisor);
    return values;
  }
  const double first = std::ceil(low / step.value);
  const double last = std::floor(high / step.value);
  if (!(last >= first)) return values;
  values.reserve(static_cast<size_t>(last - first) + 1);
  for (double i = first; i <= last; i += 1.0)
    values.push_back(i * step.value);
  return values;
}

/** The steps a time ladder is readable at, in seconds: the second,
 *  minute, hour and day divisions a clock face itself is marked in.
 *  Past a fortnight there is no such unit left and the ordinary
 *  power-of-ten rule takes over. */
constexpr std::array<double, 16> kTimeSteps = {
    1,     2,     5,     15,    30,     60,     300,    900,
    1800,  3600,  10800, 21600, 43200,  86400,  172800, 604800};

/** The readable time step for a span, in seconds. Past the last clock
 *  unit the span is measured in days and the power-of-ten rule decides,
 *  because there is no next unit to fall back on. */
double readableTimeStep(double low, double high, int count) {
  if (count <= 0 || !(high > low)) return 0.0;
  const double ideal = (high - low) / count;
  for (size_t i = 0; i + 1 < kTimeSteps.size(); ++i)
    if (ideal < std::sqrt(kTimeSteps[i] * kTimeSteps[i + 1]))
      return kTimeSteps[i];
  const Step step = readableStep(low / 86400.0, high / 86400.0, count);
  return step.valid ? step.value * 86400.0 : kTimeSteps.back();
}

bool discrete(Transform transform) {
  switch (transform) {
    case Transform::Ordinal:
    case Transform::Band:
    case Transform::Point:
      return true;
    default:
      return false;
  }
}

bool banded(Transform transform) {
  return transform == Transform::Quantize ||
         transform == Transform::Threshold;
}

/** The domain value carried onto the axis the position is proportional
 *  to. Every continuous transform is this one function plus its
 *  inverse; the mapping itself is then the same division in every
 *  case. */
double forward(const Scale& scale, double value) {
  switch (scale.transform) {
    case Transform::Log:
      return std::log(value) / std::log(scale.base);
    case Transform::Pow:
      return std::copysign(std::pow(std::abs(value), scale.exponent), value);
    case Transform::Sqrt:
      return std::copysign(std::sqrt(std::abs(value)), value);
    case Transform::Symlog:
      return std::copysign(std::log1p(std::abs(value) / scale.threshold),
                           value);
    default:
      return value;
  }
}

double backward(const Scale& scale, double axis) {
  switch (scale.transform) {
    case Transform::Log:
      return std::pow(scale.base, axis);
    case Transform::Pow:
      return std::copysign(std::pow(std::abs(axis), 1.0 / scale.exponent),
                           axis);
    case Transform::Sqrt:
      return std::copysign(axis * axis, axis);
    case Transform::Symlog:
      return std::copysign(std::expm1(std::abs(axis)) * scale.threshold, axis);
    default:
      return axis;
  }
}

double overflowed(double unit, Overflow overflow) {
  switch (overflow) {
    case Overflow::Clamp:
      return std::clamp(unit, 0.0, 1.0);
    case Overflow::Wrap: {
      const double r = std::fmod(unit, 1.0);
      return r < 0 ? r + 1.0 : r;
    }
    case Overflow::PingPong: {
      const double r = std::fmod(unit, 2.0);
      const double t = r < 0 ? r + 2.0 : r;
      return t > 1.0 ? 2.0 - t : t;
    }
    case Overflow::Extend:
      break;
  }
  return unit;
}

/** How many slots a banded or discrete transform has. */
long slotCount(const Scale& scale) {
  if (scale.transform == Transform::Threshold)
    return static_cast<long>(scale.thresholds.size()) + 1;
  return static_cast<long>(std::max(0, scale.steps));
}

/** The geometry of a banded axis: where the first entry starts and how
 *  far apart neighbouring entries are. `Point` is `Band` whose gap
 *  consumes the whole step, so one calculation serves both; `Ordinal`
 *  reads no gaps at all and pins its ends to the range's, which is the
 *  one place the three differ. */
struct Bands {
  double start = 0.0;
  double step = 0.0;
  double width = 0.0;
};

Bands bands(const Scale& scale) {
  const long n = slotCount(scale);
  const double space = scale.range.extent();
  if (n <= 0) return {scale.range.low, 0.0, 0.0};
  if (scale.transform == Transform::Ordinal)
    return {scale.range.low, n > 1 ? space / (n - 1) : 0.0, 0.0};
  const double inner =
      scale.transform == Transform::Band ? std::clamp(scale.padding, 0.0, 1.0)
                                         : 1.0;
  const double denominator = std::max(1.0, n - inner + 2.0 * scale.outerPadding);
  const double step = space / denominator;
  const double start = scale.range.low + (space - step * (n - inner)) * 0.5;
  return {start, step, step * (1.0 - inner)};
}

}  // namespace

double Scale::position(double value) const {
  if (discrete(transform)) {
    const Bands b = bands(*this);
    const double space = range.extent();
    if (space == 0.0) return 0.5;
    return overflowed((b.start + b.step * value - range.low) / space, overflow);
  }
  if (banded(transform)) {
    const long n = slotCount(*this);
    if (n <= 1) return 0.0;
    return overflowed(static_cast<double>(slot(value)) / (n - 1), overflow);
  }
  if (domain.degenerate()) return 0.5;
  const double low = forward(*this, domain.low);
  const double high = forward(*this, domain.high);
  if (high == low) return 0.5;
  return overflowed((forward(*this, value) - low) / (high - low), overflow);
}

double Scale::apply(double value) const {
  return range.low + position(value) * range.extent();
}

double Scale::invert(double at) const {
  if (discrete(transform)) {
    const Bands b = bands(*this);
    const long n = slotCount(*this);
    if (n <= 0 || b.step == 0.0) return 0.0;
    const double index = std::floor((at - b.start) / b.step);
    return std::clamp(index, 0.0, static_cast<double>(n - 1));
  }
  if (banded(transform)) {
    const long n = slotCount(*this);
    if (n <= 1) return domain.low;
    const double space = range.extent();
    const double unit = space == 0.0 ? 0.0 : (at - range.low) / space;
    const double index = std::clamp(std::floor(unit * (n - 1) + 0.5), 0.0,
                                    static_cast<double>(n - 1));
    if (transform == Transform::Threshold)
      return index <= 0 ? -std::numeric_limits<double>::infinity()
                        : thresholds[static_cast<size_t>(index) - 1];
    return domain.low + domain.extent() * (index / n);
  }
  const double space = range.extent();
  if (space == 0.0) return domain.low;
  const double unit = overflowed((at - range.low) / space, overflow);
  const double low = forward(*this, domain.low);
  const double high = forward(*this, domain.high);
  return backward(*this, low + unit * (high - low));
}

long Scale::slot(double value) const {
  const long n = slotCount(*this);
  if (n <= 0) return 0;
  if (transform == Transform::Threshold) {
    long index = 0;
    for (double cut : thresholds) {
      if (value < cut) break;
      ++index;
    }
    return index;
  }
  if (transform == Transform::Quantize) {
    if (domain.degenerate()) return 0;
    const double unit = (value - domain.low) / domain.extent();
    const double index = std::floor(unit * n);
    return static_cast<long>(std::clamp(index, 0.0, static_cast<double>(n - 1)));
  }
  if (discrete(transform))
    return static_cast<long>(
        std::clamp(std::floor(value), 0.0, static_cast<double>(n - 1)));
  return 0;
}

double Scale::bandwidth() const {
  if (transform != Transform::Band) return 0.0;
  return bands(*this).width;
}

double Scale::stepWidth() const {
  if (!discrete(transform)) return 0.0;
  return bands(*this).step;
}

double Scale::tickStep(int count) const {
  if (discrete(transform) || banded(transform)) return 0.0;
  const double low = std::min(domain.low, domain.high);
  const double high = std::max(domain.low, domain.high);
  if (transform == Transform::Time) return readableTimeStep(low, high, count);
  const Step step = readableStep(low, high, count);
  return step.valid ? step.value : 0.0;
}

std::vector<double> Scale::ticks(int count) const {
  std::vector<double> values;
  if (discrete(transform)) {
    const long n = slotCount(*this);
    values.reserve(static_cast<size_t>(std::max<long>(n, 0)));
    for (long i = 0; i < n; ++i) values.push_back(static_cast<double>(i));
    return values;
  }
  if (transform == Transform::Threshold) return thresholds;
  if (transform == Transform::Quantize) {
    const long n = slotCount(*this);
    for (long i = 1; i < n; ++i)
      values.push_back(domain.low + domain.extent() * (double(i) / n));
    return values;
  }

  const double low = std::min(domain.low, domain.high);
  const double high = std::max(domain.low, domain.high);

  if (transform == Transform::Log) {
    if (!(low > 0.0) || !(high > low) || !(base > 1.0)) return values;
    const double logBase = std::log(base);
    const double firstPower = std::floor(std::log(low) / logBase);
    const double lastPower = std::ceil(std::log(high) / logBase);
    const double decades = lastPower - firstPower;
    const double whole = std::round(base);
    // Inside a few decades every multiple of a power is worth a mark;
    // beyond that only the powers themselves fit, at whatever stride
    // keeps their number near the request.
    if (whole == base && decades < count) {
      for (double power = firstPower; power <= lastPower; power += 1.0) {
        const double decade = std::pow(base, power);
        for (double multiple = 1.0; multiple < base; multiple += 1.0) {
          const double value = multiple * decade;
          if (value >= low && value <= high) values.push_back(value);
        }
      }
      return values;
    }
    const double stride =
        std::max(1.0, std::ceil(decades / std::max(1, count)));
    for (double power = firstPower; power <= lastPower; power += stride) {
      const double value = std::pow(base, power);
      if (value >= low && value <= high) values.push_back(value);
    }
    return values;
  }

  if (transform == Transform::Time) {
    const double step = tickStep(count);
    if (step <= 0.0) return values;
    return multiples(low, high, Step{step, 0.0, true});
  }

  return multiples(low, high, readableStep(low, high, count));
}

Scale Scale::nice(int count) const {
  Scale niced = *this;
  if (discrete(transform) || banded(transform)) return niced;

  double low = std::min(domain.low, domain.high);
  double high = std::max(domain.low, domain.high);
  const bool reversed = domain.low > domain.high;

  if (transform == Transform::Log) {
    if (low > 0.0 && high > low && base > 1.0) {
      const double logBase = std::log(base);
      low = std::pow(base, std::floor(std::log(low) / logBase));
      high = std::pow(base, std::ceil(std::log(high) / logBase));
    }
    niced.domain = reversed ? Interval{high, low} : Interval{low, high};
    return niced;
  }

  double previous = 0.0;
  for (int iteration = 0; iteration < 10; ++iteration) {
    Step step;
    if (transform == Transform::Time) {
      const double seconds = readableTimeStep(low, high, count);
      step = {seconds, 0.0, seconds > 0.0};
    } else {
      step = readableStep(low, high, count);
    }
    if (!step.valid || step.value == previous) break;
    if (step.divisor != 0.0) {
      low = std::floor(low * step.divisor) / step.divisor;
      high = std::ceil(high * step.divisor) / step.divisor;
    } else {
      low = std::floor(low / step.value) * step.value;
      high = std::ceil(high / step.value) * step.value;
    }
    previous = step.value;
  }

  niced.domain = reversed ? Interval{high, low} : Interval{low, high};
  return niced;
}

}  // namespace sigil::data
