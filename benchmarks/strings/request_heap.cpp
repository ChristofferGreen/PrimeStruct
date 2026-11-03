#include <cstdio>
#include <string>
#include <vector>
static int handleRequest(int request) {
  std::vector<std::string> parts;
  for (int k = 0; k < 400; ++k) {
    std::string part = std::to_string(request * 400 + k);
    part += "/segment";
    parts.push_back(std::move(part));
  }
  int total = 0;
  for (int k = 0; k < 400; ++k) {
    total += static_cast<int>(parts[static_cast<size_t>(k)].size()) + static_cast<unsigned char>(parts[static_cast<size_t>(k)][0]);
  }
  return total;
}
int main() {
  int total = 0;
  for (int request = 0; request < 2000; ++request) {
    total += handleRequest(request);
  }
  std::printf("%d\n", total);
}
