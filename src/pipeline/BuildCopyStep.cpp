#include "primec/pipeline/BuildCopyStep.h"

#include <filesystem>
#include <system_error>

namespace primec {

bool resolveCopyDestination(const std::string &output,
                            const std::string &outDir,
                            std::string &destination,
                            std::string &error) {
  const std::filesystem::path given(output);
  if (output.empty() || given.is_absolute()) {
    error = "the copy destination \"" + output +
            "\" must be a relative path inside the output directory";
    return false;
  }
  const std::filesystem::path root = std::filesystem::path(outDir).lexically_normal();
  const std::filesystem::path target = (root / given).lexically_normal();
  const std::filesystem::path relative = target.lexically_relative(root);
  if (relative.empty() || relative == "." || *relative.begin() == "..") {
    error = "the copy destination \"" + output + "\" is not inside the output directory";
    return false;
  }
  destination = target.string();
  return true;
}

bool resolveStepOutputPath(const std::string &path,
                           const std::string &outDir,
                           std::string &resolved,
                           std::string &error) {
  if (path.empty()) {
    error = "writes an empty path";
    return false;
  }
  const std::string outside = "writes outside the output directory: " + path;
  const std::filesystem::path given(path);
  if (given.is_absolute()) {
    error = outside;
    return false;
  }
  const std::filesystem::path root = std::filesystem::path(outDir).lexically_normal();
  const std::filesystem::path target = (root / given).lexically_normal();
  const std::filesystem::path relative = target.lexically_relative(root);
  if (relative.empty() || relative == "." || *relative.begin() == "..") {
    error = outside;
    return false;
  }
  // The folders on the way may be links: the real place of the folder must still be inside the real output directory.
  std::error_code ec;
  const std::filesystem::path realRoot = std::filesystem::weakly_canonical(root, ec);
  const std::filesystem::path realParent =
      ec ? std::filesystem::path() : std::filesystem::weakly_canonical(target.parent_path(), ec);
  const std::filesystem::path realRelative =
      ec ? std::filesystem::path() : realParent.lexically_relative(realRoot);
  if (ec || realRelative.empty() || (!realRelative.empty() && *realRelative.begin() == "..")) {
    error = outside;
    return false;
  }
  if (std::filesystem::is_symlink(target, ec)) {
    error = outside;
    return false;
  }
  resolved = target.string();
  return true;
}

bool copyBuildFile(const std::string &source, const std::string &destination, std::string &error) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(source, ec)) {
    error = "the copy source " + source + " is not a file";
    return false;
  }
  const std::filesystem::path target(destination);
  std::filesystem::create_directories(target.parent_path(), ec);
  if (ec) {
    error = "cannot create " + target.parent_path().string() + ": " + ec.message();
    return false;
  }
  std::filesystem::remove(target, ec);
  ec.clear();
  std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, ec);
  if (ec) {
    error = "cannot copy " + source + " to " + destination + ": " + ec.message();
    return false;
  }
  return true;
}

} // namespace primec
