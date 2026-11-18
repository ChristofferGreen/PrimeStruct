#pragma once

#include "primec/pipeline/BuildGraphJson.h"

#include <string>
#include <vector>

namespace primec {

// Turns the link edges of a graph (`graph.links`) into what the runner executes. A step that links a library (an
// executable, a compile step or another library) gets the whole transitive closure of the libraries it links: one
// absolute archive path in `libraries` (`<outDir>/<output of the library>`, sorted by library id) and the id of each
// library in `after`, so it starts when they are built. A library that several steps link is one step of the graph,
// so it builds once. Archive paths already in `libraries` stay first.
//
// Returns the problems: an edge that names no step, an edge into something that is not a library, an edge out of
// something that is neither an executable, a compile step nor a library, and a cycle of libraries (named by the ids
// that are in it). The graph is unchanged when there is a problem.
std::vector<std::string> resolveBuildLibraryLinks(BuildGraphData &graph, const std::string &outDir);

} // namespace primec
