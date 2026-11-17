#include "primec/ir/IrLinkFiles.h"

#include "primec/ir/IrLinker.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"

#include <fstream>
#include <iterator>
#include <utility>

namespace primec {

bool loadIrModuleFile(const std::string &path, IrModule &out, std::string &error) {
  std::ifstream in(path, std::ios::binary);
  if (!in.good()) {
    error = "failed to read IR module: " + path;
    return false;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  if (!deserializeIr(bytes, out, error)) {
    error = path + ": " + error;
    return false;
  }
  return true;
}

bool linkIrModuleFiles(IrModule &module,
                       const std::vector<std::string> &paths,
                       std::string &error) {
  IrModule merged = module;
  for (const std::string &path : paths) {
    IrModule library;
    if (!loadIrModuleFile(path, library, error)) {
      return false;
    }
    if (!linkIrModule(merged, library, error)) {
      error = path + ": " + error;
      return false;
    }
  }
  if (!checkIrImportsResolved(merged, error) ||
      !validateIrModule(merged, IrValidationTarget::Vm, error)) {
    return false;
  }
  module = std::move(merged);
  return true;
}

} // namespace primec
