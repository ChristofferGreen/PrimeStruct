#include <cstdint>
#include <iostream>

static int32_t fib(int32_t n) {
  if (n < 2) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

int main() {
  std::cout << fib(32) << "\n";
  return 0;
}
