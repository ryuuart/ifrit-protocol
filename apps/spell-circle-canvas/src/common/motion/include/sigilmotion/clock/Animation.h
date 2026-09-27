#pragma once

/** @file
 * @ingroup motion-clock
 *
 * ONE TWEEN RUNNING ON ONE LIVE VALUE, for every value type an
 * `Animatable<T>` holds: the path from where the value stood (or the
 * tween's `from`) through its keyframes, written into the value's cell on
 * every step through `interpolate()`, and — for a value that adds — the
 * changes blended on top of it while it runs. One body for every `T`:
 * what differs between a number, a vector and a colour is the line
 * between two of them, which is the value's own.
 */

#include <sigilmotion/clock/Playback.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Interpolate.h>
#include <sigilmotion/values/Tween.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::motion {

namespace detail {

/** ONE TWEEN ON ONE LIVE VALUE OF `T`. */
template <Interpolable T>
class AnimationState final : public PlaybackState {
 public:
  struct Step {
    T to{};
    Duration duration{};
    Easing ease;
  };
  /** A change riding on top of the path: only a value that adds has a
   *  difference to ride. */
  struct Layer {
    Duration begin{};
    Duration duration{};
    T delta{};
    Easing ease;
  };

  /** @p tween is already resolved for the value's place among its
   *  siblings; a staggered field reads as a child alone here. */
  AnimationState(const std::shared_ptr<Cell<T>>& cell, const Tween<T>& tween)
      : m_cell(cell) {
    const Tween<T> resolved = tween.resolved({});
    m_origin = cell->value;
    if (resolved.from) m_from = resolved.from->value();
    const Duration length = resolved.duration.value();
    if (resolved.keyframes.empty()) {
      m_steps.push_back({resolved.rest(), length, resolved.easing()});
    } else {
      const Duration share = length / (double)resolved.keyframes.size();
      for (const Keyframe<T>& step : resolved.keyframes)
        m_steps.push_back({step.to, step.duration.value_or(share),
                           step.ease ? step.ease : resolved.easing()});
    }
    delay = resolved.delay.value();
    for (const Step& step : m_steps) pass += step.duration;
    passes = resolved.loop < 0 ? 0 : 1 + resolved.loop;
    alternate = resolved.alternate;
  }

  /** TAKES THE VALUE OVER: from here on this animation writes it, and a
   *  motion started on it before stops. The value holds where it stands,
   *  or at the tween's `from`. */
  void claim() {
    const std::shared_ptr<Cell<T>> cell = m_cell.lock();
    if (m_claimed || !cell) return;
    m_claimed = true;
    m_origin = cell->value;
    m_writer = ++cell->writer;
    cell->moving = true;
    if (m_from) cell->value = *m_from;
  }

  /** Rides @p delta on top of the running path, over @p duration after
   *  @p wait, from now: the value's velocity carries through. */
  void blend(T delta, Duration wait, Duration duration, Easing ease)
    requires Additive<T>
  {
    m_layers.push_back({time + wait, duration, std::move(delta), std::move(ease)});
    const Duration end = time + wait + duration;
    // A single pass runs on, holding its end, until the layer lands; the
    // path itself is unchanged, only when it completes.
    if (passes == 1 && end > total()) pass += end - total();
  }

  void showAt(Duration into, bool waiting) override {
    claim();
    const std::shared_ptr<Cell<T>> cell = m_cell.lock();
    if (!cell || cell->writer != m_writer) {
      // Every holder of the value is gone, or a motion started on the same
      // value since has taken it over.
      cancelled = true;
      return;
    }
    T value = m_from ? *m_from : m_origin;
    if (!waiting) {
      Duration remaining = into;
      for (const Step& step : m_steps) {
        if (remaining <= step.duration) {
          const float unit = step.duration > Duration{}
                                 ? (float)(remaining / step.duration)
                                 : 1.0f;
          value = interpolate(value, step.to, step.ease ? step.ease(unit) : unit);
          break;
        }
        remaining -= step.duration;
        value = step.to;
      }
    }
    if constexpr (Additive<T>) {
      for (const Layer& layer : m_layers) {
        if (time <= layer.begin) continue;
        const float unit =
            layer.duration > Duration{}
                ? (float)std::min((time - layer.begin) / layer.duration, 1.0)
                : 1.0f;
        value = value + layer.delta * (layer.ease ? layer.ease(unit) : unit);
      }
    }
    cell->value = std::move(value);
  }

  void finished() override {
    if (const auto cell = m_cell.lock(); cell && cell->writer == m_writer)
      cell->moving = false;
  }

  void revertTargets() override {
    if (const auto cell = m_cell.lock(); cell && cell->writer == m_writer) {
      cell->value = m_origin;
      cell->moving = false;
    }
  }

  /** Where the path and every change on it come to rest. */
  [[nodiscard]] T target() const
    requires Additive<T>
  {
    T value = m_steps.empty() ? m_origin : m_steps.back().to;
    for (const Layer& layer : m_layers) value = value + layer.delta;
    return value;
  }

 private:
  /** Held weakly: the value's holders own it, not the animation. */
  std::weak_ptr<Cell<T>> m_cell;
  std::optional<T> m_from;
  T m_origin{};
  std::vector<Step> m_steps;
  std::vector<Layer> m_layers;
  uint32_t m_writer = 0;
  bool m_claimed = false;
};

/** Makes @p target live, from the value it holds, if it is not. */
template <typename T>
void makeLive(Animatable<T>& target) {
  if (!target.cell()) target = animatable(target.value());
}

}  // namespace detail

/** One tween running on one live value. */
class Animation : public Playback {
 public:
  using Playback::Playback;
};

}  // namespace sigil::motion
