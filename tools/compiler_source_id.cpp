// Writes the compiler build id: a SHA-256 over the compiler's source files, as the one string literal of an include
// file that `src/support/CompilerSourceId.cpp` compiles in (include/primec/support/CompilerSourceId.h).
//
//   compiler_source_id <root> <file list> <output>
//
// The file list names one path per line, relative to `root`, in the order the build sorted them. Each file adds its
// path, its size and its bytes to the hash, so renaming, adding, removing or editing a file changes the id, and the
// id does not depend on where the tree is checked out. The output is written only when its text changes, so the unit
// that includes it is not rebuilt for a tree that hashes the same.

#include "primec/support/Sha256.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

bool readFile(const std::string &path, std::string &bytes) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "usage: compiler_source_id <root> <file list> <output>\n";
    return 2;
  }
  const std::string root = argv[1];
  std::string list;
  if (!readFile(argv[2], list)) {
    std::cerr << "compiler_source_id: cannot read the file list " << argv[2] << "\n";
    return 1;
  }
  primec::Sha256 hash;
  size_t start = 0;
  size_t files = 0;
  while (start < list.size()) {
    size_t end = list.find('\n', start);
    if (end == std::string::npos) {
      end = list.size();
    }
    const std::string relative = list.substr(start, end - start);
    start = end + 1;
    if (relative.empty()) {
      continue;
    }
    std::string bytes;
    if (!readFile(root + "/" + relative, bytes)) {
      std::cerr << "compiler_source_id: cannot read " << relative << "\n";
      return 1;
    }
    const std::string header = relative + '\0' + std::to_string(bytes.size()) + '\0';
    hash.update(header.data(), header.size());
    hash.update(bytes.data(), bytes.size());
    ++files;
  }
  if (files == 0) {
    std::cerr << "compiler_source_id: the file list is empty\n";
    return 1;
  }
  const std::string text = "\"src-" + hash.finishHex() + "\"\n";
  std::string current;
  if (readFile(argv[3], current) && current == text) {
    return 0;
  }
  std::ofstream out(argv[3], std::ios::binary | std::ios::trunc);
  out << text;
  out.flush();
  if (!out) {
    std::cerr << "compiler_source_id: cannot write " << argv[3] << "\n";
    return 1;
  }
  return 0;
}
