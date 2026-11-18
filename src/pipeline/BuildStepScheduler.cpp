#include "primec/pipeline/BuildStepScheduler.h"

#include <map>

namespace primec {

namespace {

// The index of every step by id (the first one when two share an id).
std::map<std::string, size_t> stepIndexById(const std::vector<BuildStep> &steps) {
  std::map<std::string, size_t> indices;
  for (size_t i = 0; i < steps.size(); ++i) {
    indices.emplace(steps[i].id, i);
  }
  return indices;
}

// The indices of the steps that `step` waits for; an id that is not a step is left out (`checkStepOrder` reports it).
std::vector<size_t> dependencyIndices(const BuildStep &step,
                                      const std::map<std::string, size_t> &indices) {
  std::vector<size_t> found;
  for (const std::string &id : step.after) {
    const auto it = indices.find(id);
    if (it != indices.end()) {
      found.push_back(it->second);
    }
  }
  return found;
}

} // namespace

std::vector<std::string> checkStepOrder(const std::vector<BuildStep> &steps) {
  std::vector<std::string> problems;
  const std::map<std::string, size_t> indices = stepIndexById(steps);
  std::vector<std::vector<size_t>> dependencies;
  for (const BuildStep &step : steps) {
    for (const std::string &id : step.after) {
      if (indices.find(id) == indices.end()) {
        problems.push_back("step " + step.id + " waits for " + id +
                           ", which is not a step of the graph");
      }
    }
    dependencies.push_back(dependencyIndices(step, indices));
  }
  // A step is resolved when everything it waits for is; what never resolves is in a cycle or behind one.
  std::vector<char> resolved(steps.size(), 0);
  bool progress = true;
  while (progress) {
    progress = false;
    for (size_t i = 0; i < steps.size(); ++i) {
      if (resolved[i] != 0) {
        continue;
      }
      bool allResolved = true;
      for (const size_t dependency : dependencies[i]) {
        allResolved = allResolved && resolved[dependency] != 0;
      }
      if (allResolved) {
        resolved[i] = 1;
        progress = true;
      }
    }
  }
  for (size_t i = 0; i < steps.size(); ++i) {
    if (resolved[i] == 0) {
      problems.push_back("step " + steps[i].id +
                         " is in a cycle of after edges or waits behind one");
    }
  }
  return problems;
}

BuildStepScheduler::BuildStepScheduler(const std::vector<BuildStep> &steps,
                                       const std::vector<bool> &settled)
    : steps_(steps), started_(steps.size(), 0), succeeded_(steps.size(), 0) {
  const std::map<std::string, size_t> indices = stepIndexById(steps);
  for (size_t i = 0; i < steps.size(); ++i) {
    dependencies_.push_back(dependencyIndices(steps[i], indices));
    if (i < settled.size() && settled[i]) {
      started_[i] = 1;
      succeeded_[i] = 1;
    }
  }
}

bool BuildStepScheduler::ready(size_t index) const {
  for (const size_t dependency : dependencies_[index]) {
    if (succeeded_[dependency] == 0) {
      return false;
    }
  }
  return true;
}

size_t BuildStepScheduler::take() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (!stopped_) {
    bool unstarted = false;
    for (size_t i = 0; i < steps_.size(); ++i) {
      if (started_[i] != 0) {
        continue;
      }
      unstarted = true;
      if (ready(i)) {
        started_[i] = 1;
        ++running_;
        return i;
      }
    }
    // Nothing left, or nothing running that could make a waiting step ready (its dependency failed).
    if (!unstarted || running_ == 0) {
      return None;
    }
    changed_.wait(lock);
  }
  return None;
}

void BuildStepScheduler::finish(size_t index, bool succeeded) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    succeeded_[index] = succeeded ? 1 : 0;
    --running_;
  }
  changed_.notify_all();
}

void BuildStepScheduler::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
  }
  changed_.notify_all();
}

} // namespace primec
