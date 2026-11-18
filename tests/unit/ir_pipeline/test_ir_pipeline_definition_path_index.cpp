#include "validation/test_ir_pipeline_validation_helpers.h"

#include "primec/ir_lowerer/IrLowererDefinitionPathIndex.h"

#include <string>
#include <unordered_map>
#include <vector>

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

struct IndexedDefinitions {
  std::vector<primec::Definition> definitions;
  std::unordered_map<std::string, const primec::Definition *> defMap;

  explicit IndexedDefinitions(const std::vector<std::string> &paths, const std::vector<std::string> &sumPaths = {}) {
    definitions.reserve(paths.size());
    for (const std::string &path : paths) {
      primec::Definition def;
      def.fullPath = path;
      for (const std::string &sumPath : sumPaths) {
        if (sumPath == path) {
          primec::Transform sum;
          sum.name = "sum";
          def.transforms.push_back(sum);
        }
      }
      definitions.push_back(std::move(def));
    }
    for (const primec::Definition &def : definitions) {
      defMap.emplace(def.fullPath, &def);
    }
  }
};

// What a scan of the map returns: the first definition, in the map's iteration order, whose path is `lookupPath`
// followed by a generated-leaf marker and no further `/`.
const primec::Definition *scanForGeneratedLeaf(
    const std::unordered_map<std::string, const primec::Definition *> &defMap, const std::string &lookupPath) {
  auto matches = [&](const std::string &path, const char *marker, size_t markerSize) {
    return path.rfind(lookupPath, 0) == 0 && path.compare(lookupPath.size(), markerSize, marker) == 0 &&
           path.find('/', lookupPath.size() + markerSize) == std::string::npos;
  };
  for (const auto &[path, def] : defMap) {
    if (def != nullptr && (matches(path, "__t", 3) || matches(path, "__ov", 4) || matches(path, "<", 1))) {
      return def;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("definition path index finds generated leaf definitions like a scan of the map") {
  const IndexedDefinitions defs({"/std/http/route__t1a2b",
                                 "/std/http/route__t9f00",
                                 "/std/http/route__ov3",
                                 "/std/http/route<i32>",
                                 "/std/http/routed__t1",
                                 "/std/http/route__t1/inner",
                                 "/std/http/other",
                                 "/main"});
  const primec::ir_lowerer::DefinitionPathIndex index(defs.defMap);
  for (const std::string lookup : {"/std/http/route", "/std/http/rout", "/std/http/other", "/main", "/std", "/"}) {
    CAPTURE(lookup);
    CHECK(index.findGeneratedLeafDefinition(lookup) == scanForGeneratedLeaf(defs.defMap, lookup));
  }
  CHECK(index.findGeneratedLeafDefinition("/std/http/route") != nullptr);
}

TEST_CASE("definition path index rejects a generated leaf with a nested path") {
  const IndexedDefinitions defs({"/a/helper__t1/nested", "/a/helper_t1", "/a/helpers"});
  const primec::ir_lowerer::DefinitionPathIndex index(defs.defMap);
  CHECK(index.findGeneratedLeafDefinition("/a/helper") == nullptr);
  CHECK(index.findGeneratedLeafDefinition("/missing") == nullptr);
}

TEST_CASE("definition path index lists sum definition paths sorted") {
  const IndexedDefinitions defs({"/z/Shape", "/a/Option", "/plain", "/m/Result"}, {"/z/Shape", "/a/Option", "/m/Result"});
  const primec::ir_lowerer::DefinitionPathIndex index(defs.defMap);
  const std::vector<std::string> expected = {"/a/Option", "/m/Result", "/z/Shape"};
  CHECK(index.sumDefinitionPaths() == expected);
}

TEST_SUITE_END();
