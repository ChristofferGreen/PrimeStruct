// The host side of text_host.prime for a compiled program: the same behavior as the bindings the
// VM test installs.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {

const char *host_greet(const char *name) {
  static std::string greeting;
  greeting = std::string("hello ") + name;
  return greeting.c_str();
}

const char *host_empty(void) { return ""; }

int32_t host_measure(const char *text) { return static_cast<int32_t>(std::strlen(text)); }

void host_note(const char *text) { std::printf("note %s\n", text); }
}
