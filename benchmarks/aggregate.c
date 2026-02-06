#include <stdint.h>
#include <stdio.h>

int main(void) {
  const int64_t n = 5000000;
  int64_t sum = 0;
  int64_t sumsq = 0;
  for (int64_t i = 0; i < n; ++i) {
    sum += i;
    sumsq += i * i;
  }
  printf("%lld\n", (long long)(sum + sumsq));
  return 0;
}
