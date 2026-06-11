#pragma once

#include <string>

inline std::string reflectionCodegenDumpSource() {
  return R"(
[struct reflect generate(Clone, DebugPrint, IsDefault, Default, NotEqual, Equal)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int> effects(io_out)]
main() {
  [Pair] baseline{/Pair/Default()}
  [Pair] copy{/Pair/Clone(baseline)}
  /Pair/DebugPrint(copy)
  [bool] equalCopy{/Pair/Equal(copy, baseline)}
  [bool] notEqual{/Pair/NotEqual(copy, Pair{[x] 9i32, [y] 2i32})}
  [bool] isDefault{/Pair/IsDefault(baseline)}
  return(if(and(and(equalCopy, notEqual), isDefault), then() { 0i32 }, else() { 1i32 }))
}
)";
}

inline std::string reflectionCodegenRuntimeSource() {
  return R"(
[struct reflect generate(Equal, NotEqual, Default, IsDefault, Clone, DebugPrint)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int> effects(io_out)]
main() {
  [Pair] baseline{/Pair/Default()}
  [Pair] copy{/Pair/Clone(baseline)}
  /Pair/DebugPrint(copy)
  [bool] equalCopy{/Pair/Equal(copy, baseline)}
  [bool] notEqual{/Pair/NotEqual(copy, Pair{[x] 9i32, [y] 2i32})}
  [bool] isDefault{/Pair/IsDefault(baseline)}
  return(if(and(and(equalCopy, notEqual), isDefault), then() { 7i32 }, else() { 3i32 }))
}
)";
}
