#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
  std::vector<int64_t> values;
  for (int32_t i = 0; i < 1000; ++i) {
    values.push_back(i);
  }
  int64_t total = 0;
  for (int32_t pass = 0; pass < 20000; ++pass) {
    for (int32_t index = 0; index < 1000; ++index) {
      total += values[static_cast<size_t>(index)];
    }
  }
  std::printf("%lld\n", static_cast<long long>(total));
  return 0;
}
