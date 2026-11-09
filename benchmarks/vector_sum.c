#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  const int32_t count = 1000;
  int64_t *values = malloc(sizeof(int64_t) * (size_t)count);
  if (values == NULL) {
    return 1;
  }
  for (int32_t i = 0; i < count; ++i) {
    values[i] = i;
  }
  int64_t total = 0;
  for (int32_t pass = 0; pass < 20000; ++pass) {
    for (int32_t index = 0; index < count; ++index) {
      total += values[index];
    }
  }
  printf("%lld\n", (long long)total);
  free(values);
  return 0;
}
