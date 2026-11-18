#pragma once

#include "primec/pipeline/BuildGraphJson.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace primec {

// The order in which `primec build` starts the steps of one graph (docs/BuildSystemPlan.md section 4): a step starts
// only when every step it names in `after` has finished successfully. Steps without `after` edges start in the order
// of the graph, as before.

// The problems of the `after` edges of `steps`: one line for every edge that names an id that is not a step of the
// graph, and one for every step that is in a cycle of edges or waits behind one. Empty when the edges can be followed.
std::vector<std::string> checkStepOrder(const std::vector<BuildStep> &steps);

// Hands steps to worker threads. `settled[i]` is true for a step that finished before the run (an up to date function
// step); a step that is not settled is handed out once, when its dependencies are done.
class BuildStepScheduler {
public:
  static constexpr size_t None = static_cast<size_t>(-1);

  BuildStepScheduler(const std::vector<BuildStep> &steps, const std::vector<bool> &settled);

  // The next step that can start, waiting while every unstarted step waits for a running one. `None` when nothing is
  // left to start, when `stop()` was called, or when a dependency failed (the steps behind it never start).
  size_t take();

  // Reports that step `index` ended, successfully or not, and wakes the workers that wait for it.
  void finish(size_t index, bool succeeded);

  // No further step starts; a worker that waits returns.
  void stop();

private:
  bool ready(size_t index) const;

  const std::vector<BuildStep> &steps_;
  std::vector<std::vector<size_t>> dependencies_;
  std::vector<char> started_;
  std::vector<char> succeeded_;
  size_t running_ = 0;
  bool stopped_ = false;
  std::mutex mutex_;
  std::condition_variable changed_;
};

} // namespace primec
