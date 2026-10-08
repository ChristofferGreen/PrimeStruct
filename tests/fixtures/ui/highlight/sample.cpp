#include <vector>

// counts items
int count_items(const std::vector<int> &items) {
  /* total */
  int total = 0;
  for (int item : items) {
    total += item;
  }
  const char *label = "items: 42";
  return total > 42 ? 1 : 0;
}
