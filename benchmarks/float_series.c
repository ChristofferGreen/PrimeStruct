#include <stdint.h>
#include <stdio.h>

int main(void) {
  const int32_t n = 20000000;
  double sum = 0.0;
  double sign = 1.0;
  double denominator = 1.0;
  for (int32_t i = 0; i < n; ++i) {
    sum += sign / denominator;
    sign = -sign;
    denominator += 2.0;
  }
  printf("%lld\n", (long long)(sum * 4000000000.0));
  return 0;
}
