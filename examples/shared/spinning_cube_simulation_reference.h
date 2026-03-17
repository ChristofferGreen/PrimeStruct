#pragma once

#include <cmath>

namespace primestruct::spinning_cube_simulation {

struct State {
  int tick = 0;
  float angleRadians = 0.0f;
  float angularVelocity = 1.0471976f;
  float axisX = 1.0f;
  float axisY = 0.0f;
};

struct StepResult {
  State state;
  float remainingSeconds = 0.0f;
  int stepCount = 0;
};

inline float wrapAngle(float angle) {
  constexpr float Tau = 6.2831853f;
  float wrapped = angle;
  while (wrapped >= Tau) {
    wrapped -= Tau;
  }
  while (wrapped < 0.0f) {
    wrapped += Tau;
  }
  return wrapped;
}

inline State tick(const State &state, float deltaSeconds) {
  State next = state;
  next.tick += 1;
  next.angleRadians = wrapAngle(state.angleRadians + state.angularVelocity * deltaSeconds);
  next.axisX = state.axisX * 0.9998477f + state.axisY * -0.0174524f;
  next.axisY = state.axisX * 0.0174524f + state.axisY * 0.9998477f;
  return next;
}

inline StepResult advanceFixed(const State &state, float accumulatedSeconds, float fixedDeltaSeconds, int maxSteps) {
  StepResult result;
  result.state = state;
  result.remainingSeconds = accumulatedSeconds;
  if (fixedDeltaSeconds <= 0.0f || maxSteps < 1) {
    return result;
  }
  while (result.remainingSeconds >= fixedDeltaSeconds) {
    if (result.stepCount >= maxSteps) {
      return result;
    }
    result.state = tick(result.state, fixedDeltaSeconds);
    result.remainingSeconds -= fixedDeltaSeconds;
    result.stepCount += 1;
  }
  return result;
}

inline State runFixedTicks(int ticks) {
  State state;
  constexpr float FixedDelta = 0.016666667f;
  for (int i = 0; i < ticks; ++i) {
    const StepResult step = advanceFixed(state, FixedDelta, FixedDelta, 1);
    state = step.state;
  }
  return state;
}

inline State runFixedTicksChunked(int ticks) {
  State state;
  constexpr float FixedDelta = 0.016666667f;
  float accumulator = 0.0f;
  for (int i = 0; i < ticks; ++i) {
    accumulator += 0.010000000f;
    const StepResult stepA = advanceFixed(state, accumulator, FixedDelta, 4);
    state = stepA.state;
    accumulator = stepA.remainingSeconds;

    accumulator += 0.006666667f;
    const StepResult stepB = advanceFixed(state, accumulator, FixedDelta, 4);
    state = stepB.state;
    accumulator = stepB.remainingSeconds;
  }
  return state;
}

inline int snapshotCode(const State &state) {
  const int tickBucket = static_cast<int>(static_cast<float>(state.tick) * 0.1f);
  const int angleDeci = static_cast<int>(state.angleRadians * 10.0f);
  const int axisXShiftedDeci = static_cast<int>((state.axisX + 1.0f) * 10.0f);
  const int axisYDeci = static_cast<int>(state.axisY * 10.0f);
  return tickBucket + angleDeci + axisXShiftedDeci + axisYDeci;
}

inline int snapshotCodeForTicks(int ticks) {
  return snapshotCode(runFixedTicks(ticks));
}

inline bool parityOk(int ticks) {
  const State fixed = runFixedTicks(ticks);
  const State chunked = runFixedTicksChunked(ticks);
  return std::fabs(fixed.angleRadians - chunked.angleRadians) <= 0.02f &&
         std::fabs(fixed.axisX - chunked.axisX) <= 0.02f &&
         std::fabs(fixed.axisY - chunked.axisY) <= 0.02f;
}

} // namespace primestruct::spinning_cube_simulation
