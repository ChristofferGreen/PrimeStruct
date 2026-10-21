// The host side of scalars.prime for the C++ emitters, linked with --link-arg. The test
// binds the same behavior for the VM, so both runs print the same lines.
#include <cstdint>
#include <cstdio>

extern "C" {

void host_note(const char *label, uint64_t id) { std::printf("note %s %llu\n", label, static_cast<unsigned long long>(id)); }

int32_t host_add(int32_t left, int32_t right) { return left + right; }

int64_t host_widen(int64_t value) { return value * 3; }

bool host_flag(bool on) { return !on; }

float host_half(float value) { return value / 2.0f; }

double host_scale(double value, double factor) { return value * factor; }

}
