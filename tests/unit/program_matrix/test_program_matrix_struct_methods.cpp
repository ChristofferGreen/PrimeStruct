#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.struct_methods");

// Methods of generic structs and overloaded struct methods (docs/spec/language-core.md, Helper overloading). These
// programs run on the VM kernels, optexe and native, the backends web servers and apps are built with.

namespace {

std::vector<std::string> methodConfigs() {
  std::vector<std::string> configs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    configs.push_back("native-O0");
    configs.push_back("native-O2");
  }
  return configs;
}

void runMethodProgram(const std::string &name, const std::string &source, int exitCode) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = exitCode;
  program.onlyConfigs = methodConfigs();
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("a generic struct's method calls clear on a reference parameter, a local and a vector field") {
  runMethodProgram("struct_methods_generic_clear", R"(
import /std/collections/*

[struct]
Cell() {
  [i32 mut] n{5i32}

  [mut]
  clear() {
    this.n = 0i32
  }
}

[struct]
Holder<T>() {
  [vector<i32> mut] items{vector<i32>()}
  [Cell mut] own{Cell{}}

  [mut effects(heap_alloc)]
  fill() {
    this.items.push(1i32)
    this.items.push(2i32)
  }

  [mut]
  wipeParam([Reference<Cell>] cell) {
    cell.clear()
  }

  [mut]
  wipeLocal() {
    [Cell mut] mine{Cell{}}
    mine.clear()
    this.own.n = mine.n + 7i32
  }

  [mut effects(heap_alloc)]
  wipeField() {
    this.items.clear()
    this.own.clear()
  }

  [return<i32>]
  total() {
    return(this.items.count() + this.own.n)
  }
}

[return<int> effects(heap_alloc)]
main() {
  [Holder<i32> mut] h{Holder<i32>{}}
  h.fill()
  [Cell mut] shared{Cell{}}
  h.wipeParam(shared)
  h.wipeLocal()
  [i32] before{h.total()}
  h.wipeField()
  return(before * 10i32 + h.total() + shared.n)
}
)",
                   90);
}
