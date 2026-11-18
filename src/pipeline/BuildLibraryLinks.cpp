#include "primec/pipeline/BuildLibraryLinks.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

namespace primec {

namespace {

using Edges = std::map<std::string, std::set<std::string>>;

// Adds every library reachable from `id` to `closure`; `visiting` holds the ids on the current path, and a library
// reached while it is on the path is a cycle, whose members go to `cyclic`.
void collectClosure(const std::string &id,
                    const Edges &edges,
                    std::set<std::string> &closure,
                    std::set<std::string> &visiting,
                    std::set<std::string> &cyclic) {
  const auto direct = edges.find(id);
  if (direct == edges.end()) {
    return;
  }
  visiting.insert(id);
  for (const std::string &library : direct->second) {
    if (visiting.count(library) != 0) {
      cyclic.insert(library);
      cyclic.insert(id);
      continue;
    }
    if (closure.insert(library).second) {
      collectClosure(library, edges, closure, visiting, cyclic);
    }
  }
  visiting.erase(id);
}

std::string archivePath(const BuildStep &library, const std::string &outDir) {
  return (std::filesystem::path(outDir) / library.output).lexically_normal().string();
}

} // namespace

std::vector<std::string> resolveBuildLibraryLinks(BuildGraphData &graph,
                                                  const std::string &outDir) {
  std::vector<std::string> problems;
  std::map<std::string, const BuildStep *> byId;
  for (const BuildStep &step : graph.steps) {
    byId.emplace(step.id, &step);
  }
  Edges edges;
  for (const BuildLink &link : graph.links) {
    const auto from = byId.find(link.from);
    const auto to = byId.find(link.to);
    if (from == byId.end() || to == byId.end()) {
      problems.push_back("link " + link.from + " -> " + link.to +
                         " names a step that is not in the graph");
      continue;
    }
    if (to->second->kind != "library") {
      problems.push_back("link " + link.from + " -> " + link.to + ": " + link.to +
                         " is not a library");
      continue;
    }
    const std::string &kind = from->second->kind;
    if (kind != "library" && kind != "executable" && kind != "compile") {
      problems.push_back("link " + link.from + " -> " + link.to + ": " + link.from +
                         " is not an executable or a library");
      continue;
    }
    edges[link.from].insert(link.to);
  }
  std::map<std::string, std::set<std::string>> closures;
  std::set<std::string> cyclic;
  for (const auto &entry : edges) {
    std::set<std::string> closure;
    std::set<std::string> visiting;
    collectClosure(entry.first, edges, closure, visiting, cyclic);
    closures.emplace(entry.first, std::move(closure));
  }
  for (const std::string &id : cyclic) {
    problems.push_back("library " + id + " is in a cycle of libraries");
  }
  if (!problems.empty()) {
    return problems;
  }
  for (BuildStep &step : graph.steps) {
    const auto closure = closures.find(step.id);
    if (closure == closures.end()) {
      continue;
    }
    for (const std::string &id : closure->second) {
      const std::string path = archivePath(*byId.at(id), outDir);
      if (std::find(step.libraries.begin(), step.libraries.end(), path) == step.libraries.end()) {
        step.libraries.push_back(path);
      }
      if (std::find(step.after.begin(), step.after.end(), id) == step.after.end()) {
        step.after.push_back(id);
      }
    }
  }
  return problems;
}

} // namespace primec
