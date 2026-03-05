#include <cstdlib>
#include <iostream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {

std::string shellQuote(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

int decodeExitCode(int rawCode) {
#if defined(__unix__) || defined(__APPLE__)
  if (rawCode == -1) {
    return -1;
  }
  if (WIFEXITED(rawCode)) {
    return WEXITSTATUS(rawCode);
  }
  return -1;
#else
  return rawCode;
#endif
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: spinning_cube_host <cube_native_binary>\n";
    return 64;
  }

  const std::string cubeBinaryPath = argv[1];
  const std::string command = shellQuote(cubeBinaryPath);
  const int cubeExitCode = decodeExitCode(std::system(command.c_str()));
  if (cubeExitCode < 0) {
    std::cerr << "failed to execute cube binary\n";
    return 70;
  }

  if (cubeExitCode != 36) {
    std::cerr << "unexpected cube exit code: " << cubeExitCode << "\n";
    return 1;
  }

  std::cout << "native host verified cube simulation output\n";
  return 0;
}
