#include <stdint.h>
#include <stdio.h>

static int32_t fib(int32_t n) {
  if (n < 2) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

int main(void) {
  printf("%d\n", fib(32));
  return 0;
}
