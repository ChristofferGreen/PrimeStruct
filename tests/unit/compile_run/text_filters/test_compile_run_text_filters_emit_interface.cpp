// `primec --emit=interface` and `--emit=symbols` (docs/spec/integration-and-tooling.md, Module Interface): the
// declaration printer and the symbol table of a module that has no entry point.
#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

// A module with a struct, an enum, functions (one without a written return type, one private), a nested namespace and
// no `main`.
const std::string ModuleSource = R"(
[namespace summary("Drawing.")]
ui {
  [public struct summary("A point on the grid.")]
  Point() {
    [public i32] x{1i32}
    [public i32] y{2i32}
    [private i32] secret{3i32}
    [public return<i32>]
    sum([Point] self) { return(self.x + self.y) }
  }

  [public enum]
  Color {
    Red
    Green = 5
  }

  [public effects(io_out) return<i32> summary("Adds two numbers.")]
  add([i32] a, [i32] b{2i32}) {
    return(a + b)
  }

  [public]
  inferred() {
    return(7i32)
  }

  [return<i32>]
  hidden() {
    return(1i32)
  }

  [namespace]
  inner {
    [public return<bool>]
    deep() { return(true) }
  }
}
)";

const std::string ExpectedInterface = R"([namespace summary("Drawing.")] ui {
  [public, enum] Color {
    Red = 0i32
    Green = 5i32
  }

  [public, struct, summary("A point on the grid.")] Point() {
    [public, i32] x{1i32}
    [public, i32] y{2i32}
    [private, i32] secret{3i32}
    [public, return<i32>, imported(emit_interface_golden)] sum([/ui/Point] self) {}
  }

  [public, effects(io_out), return<i32>, imported(emit_interface_golden), summary("Adds two numbers.")] add([i32] a, [i32] b{2i32}) {}

  [public, return<i32>, imported(emit_interface_golden)] inferred() {}

  [namespace] inner {
    [public, return<bool>, imported(emit_interface_golden)] deep() {}
  }
}
)";

const std::string ExpectedSymbols =
    R"({"interfaceHash":"6a2272927b91f1ff25f664c916dbb465c96bd2c249e20d7e634fb06bcdff2971","symbols":[
{"path":"/ui/Color","kind":"enum","signature":"1aab50e10f98c0c75e4c8c6604d78352d1764849df4fbc070640362355058a02","content":"fa6da5a05dae4b3ec71a6cd93def74862c30b5f852a61a02e4fdee386f835e1b","closure":"9ead24982a2d2d361883be611405d689bb42998d902cce5a161a301259b554b9"},
{"path":"/ui/Point","kind":"struct","signature":"201683163b9940949f39f0c40219bb0c01e00fea87681f1c2aad96fe8b2351fd","content":"9e772ebaa89b5078d1fa2a8bb9185fbf300ecbf9156b9d78e02e2c94a05d3e9f","closure":"17d54cdea8e2cea093bfbf86702b479c91e825bf5011b5ce9ec2b289408154e7"},
{"path":"/ui/Point/sum","kind":"function","signature":"78b4448ca0821f428e7299133ddb171568d40186fb1d40ddef89543a41c00d17","content":"43edcd44d10417b873b376fc2cd1c59ded2d25a37d9de179c67710bf070b8a55","closure":"b1810f7cadd5afa8afc4f7a399414d9f0ed57afa13d95eb20dd432d2b2787405"},
{"path":"/ui/add","kind":"function","signature":"f6491b80d09e1dad813c08745de5ee8c8566d0f28996fab9e832d3a365e90f0f","content":"dd6317c0ac407da82b238d954159a4265b592843619e1242ccce7d57af93ebc3","closure":"4473b258040824a50395092c5230dcf4fd3a1cb480e9b26798695084121b11a7"},
{"path":"/ui/inferred","kind":"function","signature":"44ae00e790fa2363e7be086f25e8fc01f2a9bc7ba39f885c6b664490d5f30a53","content":"ae34869427dc8676284d22793e36a4c015c9c7359c3f66e0605f90fcf10351c7","closure":"85073df034bb21db0c8bbd0bc3e2862706e103fd9de1799b95825baf3cefe71f"},
{"path":"/ui/inner/deep","kind":"function","signature":"54d27e81b295d47e418ed5ac1d0947e3cc265efc9354532dd874d66f225af954","content":"18cd5240b9c126b475d8d95cc3ea04326527a553ca5bd40f9ce1cee7cfb9126e","closure":"5d67bf60e66b6ae4a34d2c972f64d7450e4d3f9dd482b0cd39c1897020febdb2"}
]}
)";

struct InterfaceRun {
  int exitCode = 0;
  std::string out;
  std::string err;
};

InterfaceRun runEmit(const std::string &name, const std::string &source, const std::string &kind) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + "_" + kind + "_out.txt")).string();
  const std::string errPath = (testScratchPath("") / (name + "_" + kind + "_err.txt")).string();
  InterfaceRun run;
  run.exitCode = runCommand("./primec --emit=" + kind + " " + quoteShellArg(srcPath) + " > " +
                            quoteShellArg(outPath) + " 2> " + quoteShellArg(errPath));
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  return run;
}

// The hash `field` of the table line of `path`; empty when the symbol is not in the table.
std::string
tableField(const std::string &table, const std::string &path, const std::string &field) {
  const size_t line = table.find("{\"path\":\"" + path + "\"");
  if (line == std::string::npos) {
    return "";
  }
  const std::string key = "\"" + field + "\":\"";
  const size_t start = table.find(key, line);
  if (start == std::string::npos) {
    return "";
  }
  return table.substr(start + key.size(), table.find('"', start + key.size()) - start - key.size());
}

std::string interfaceHashOf(const std::string &table) {
  const std::string key = "\"interfaceHash\":\"";
  const size_t start = table.find(key);
  return start == std::string::npos ? "" : table.substr(start + key.size(), 64);
}

// A module of two functions, `helper` called by `caller`.
std::string helperModule(const std::string &helperBody) {
  return "[public return<i32>]\ncaller() {\n  return(helper() + "
         "1i32)\n}\n\n[return<i32>]\nhelper() {\n  return(" +
         helperBody + ")\n}\n";
}

} // namespace

TEST_CASE("emit-interface prints the interface of a module without an entry point") {
  const InterfaceRun run = runEmit("emit_interface_golden", ModuleSource, "interface");
  CHECK(run.exitCode == 0);
  CHECK(run.err.empty());
  CHECK(run.out == ExpectedInterface);
  CHECK(run.out.find("hidden") == std::string::npos);
  CHECK(run.out.find("secret") != std::string::npos);
}

TEST_CASE("emit-symbols prints the symbol table of a module") {
  const InterfaceRun run = runEmit("emit_interface_golden", ModuleSource, "symbols");
  CHECK(run.exitCode == 0);
  CHECK(run.out == ExpectedSymbols);
  CHECK(run.out.find("hidden") == std::string::npos);
}

TEST_CASE("emit-interface leaves out the entry point and the stdlib") {
  const InterfaceRun run =
      runEmit("emit_interface_entry",
              "import /std/collections/vector\n\n[public effects(heap_alloc) "
              "return<i32>]\ncount_items() {\n"
              "  [vector<i32> mut] v{vector<i32>{}}\n  v.push(3i32)\n  return(v.count())\n}\n\n"
              "[public return<int>]\nmain() {\n  return(0i32)\n}\n",
              "interface");
  CHECK(run.exitCode == 0);
  CHECK(run.out == "[public, effects(heap_alloc), return<i32>, imported(emit_interface_entry)] "
                   "count_items() {}\n");
}

TEST_CASE("emit-interface prints inferred return types resolved") {
  const InterfaceRun run =
      runEmit("emit_interface_inferred",
              "[public]\nwide() {\n  return(7i64)\n}\n\n[public]\nflag() {\n  return(true)\n}\n\n"
              "[public]\nsum([i32] a) {\n  return(a + 1i32)\n}\n",
              "interface");
  CHECK(run.exitCode == 0);
  CHECK(run.out.find("[public, return<bool>, imported(emit_interface_inferred)] flag() {}") !=
        std::string::npos);
  CHECK(run.out.find("[public, return<i32>, imported(emit_interface_inferred)] "
                     "sum([i32] a) {}") != std::string::npos);
  CHECK(run.out.find("[public, return<i64>, imported(emit_interface_inferred)] wide() {}") !=
        std::string::npos);
}

TEST_CASE("emit-interface printed twice is byte-identical") {
  const InterfaceRun first = runEmit("emit_interface_twice", ModuleSource, "interface");
  const InterfaceRun second = runEmit("emit_interface_twice", ModuleSource, "interface");
  CHECK(first.out == second.out);
  const InterfaceRun firstTable = runEmit("emit_interface_twice", ModuleSource, "symbols");
  const InterfaceRun secondTable = runEmit("emit_interface_twice", ModuleSource, "symbols");
  CHECK(firstTable.out == secondTable.out);
  CHECK(firstTable.out.size() > 100);
}

TEST_CASE("emit-symbols: a body change moves the content hash and not the signature or the "
          "interface hash") {
  const InterfaceRun before = runEmit("emit_interface_body_a", helperModule("1i32"), "symbols");
  const InterfaceRun after = runEmit("emit_interface_body_b", helperModule("2i32"), "symbols");
  REQUIRE(before.exitCode == 0);
  REQUIRE(after.exitCode == 0);
  CHECK(interfaceHashOf(before.out).size() == 64);
  CHECK(interfaceHashOf(before.out) == interfaceHashOf(after.out));
  CHECK(tableField(before.out, "/caller", "signature").size() == 64);
  CHECK(tableField(before.out, "/caller", "signature") ==
        tableField(after.out, "/caller", "signature"));
  // The private helper is not in the table, and the public caller's own text is the same ...
  CHECK(before.out.find("/helper") == std::string::npos);
  CHECK(tableField(before.out, "/caller", "content") ==
        tableField(after.out, "/caller", "content"));
  // ... but its closure holds the helper, so the closure hash moves.
  CHECK(tableField(before.out, "/caller", "closure") !=
        tableField(after.out, "/caller", "closure"));
}

TEST_CASE("emit-symbols: a changed body of a public function moves its content hash only") {
  const std::string body =
      "[public return<i32>]\nvalue() {\n  return(1i32)\n}\n\n[public return<i32>]\nother() {\n"
      "  return(5i32)\n}\n";
  const std::string changed =
      "[public return<i32>]\nvalue() {\n  return(9i32)\n}\n\n[public return<i32>]\nother() {\n"
      "  return(5i32)\n}\n";
  const InterfaceRun before = runEmit("emit_interface_value_a", body, "symbols");
  const InterfaceRun after = runEmit("emit_interface_value_b", changed, "symbols");
  CHECK(tableField(before.out, "/value", "content") != tableField(after.out, "/value", "content"));
  CHECK(tableField(before.out, "/value", "signature") ==
        tableField(after.out, "/value", "signature"));
  CHECK(tableField(before.out, "/other", "content") == tableField(after.out, "/other", "content"));
  CHECK(tableField(before.out, "/other", "closure") == tableField(after.out, "/other", "closure"));
  CHECK(interfaceHashOf(before.out) == interfaceHashOf(after.out));
}

TEST_CASE("emit-symbols: a changed signature moves the signature hash and the interface hash") {
  const InterfaceRun before = runEmit(
      "emit_interface_sig_a", "[public return<i32>]\nvalue() {\n  return(1i32)\n}\n", "symbols");
  const InterfaceRun after = runEmit(
      "emit_interface_sig_b", "[public return<i64>]\nvalue() {\n  return(1i64)\n}\n", "symbols");
  CHECK(tableField(before.out, "/value", "signature") !=
        tableField(after.out, "/value", "signature"));
  CHECK(interfaceHashOf(before.out) != interfaceHashOf(after.out));
}

TEST_CASE("emit-interface prints structs and enums that read back unchanged") {
  const std::string typesOnly =
      "[public struct]\nPoint() {\n  [public i32] x{1i32}\n  [private i64] y{2i64}\n}\n\n"
      "[public enum<i64>]\nShade {\n  Light = 1i64\n  Dark = 2i64\n}\n";
  const InterfaceRun first = runEmit("emit_interface_types", typesOnly, "interface");
  REQUIRE(first.exitCode == 0);
  // The printed text is a program: printing it again gives the same text.
  const InterfaceRun second = runEmit("emit_interface_types_again", first.out, "interface");
  CHECK(second.exitCode == 0);
  CHECK(second.out == first.out);
  CHECK(first.out.find("[public, enum<i64>] Shade {") != std::string::npos);
}

TEST_CASE("emit-interface prints a generic in full with the private definitions it reaches") {
  const std::string source = "[namespace]\nui {\n"
                             "  [return<i32>]\n  helper([i32] a) { return(a + 1i32) }\n"
                             "  [return<i32>]\n  unused() { return(2i32) }\n"
                             "  [public return<i32>]\n  bump<T>([T] a) { return(helper(3i32)) }\n"
                             "}\n";
  const InterfaceRun run = runEmit("emit_interface_generic", source, "interface");
  REQUIRE(run.exitCode == 0);
  CHECK(run.out.find("[public, return<i32>] bump<T>([T] a) {\n    return(helper(3i32))\n  }") !=
        std::string::npos);
  // What the generic reaches is declared, still private; what nothing reaches is not printed.
  CHECK(run.out.find("[return<i32>, imported(emit_interface_generic)] helper([i32] a) {}") !=
        std::string::npos);
  CHECK(run.out.find("unused") == std::string::npos);
}

TEST_CASE("emit-interface reports a semantic error of the module") {
  const InterfaceRun run = runEmit("emit_interface_error",
                                   "[public return<i32>]\nvalue() {\n  return(missing_name)\n}\n",
                                   "interface");
  CHECK(run.exitCode == 2);
  CHECK(run.out.empty());
  CHECK(run.err.find("missing_name") != std::string::npos);
}

TEST_SUITE_END();
