#version 450
layout(std430, binding = 0) buffer PrimeStructOutput {
  int value;
} ps_output;
int ps_entry_0(inout int stack[1024], inout int sp);
int ps_entry_0(inout int stack[1024], inout int sp) {
  int locals[1024];
  for (int i = 0; i < 1024; ++i) {
    locals[i] = 0;
  }
  int pc = 0;
  while (true) {
    switch (pc) {
      case 0: {
        // Narrowed GLSL path lowers f64 literals through f32 payloads.
        stack[sp++] = int(uint(1069547520u));
        pc = 1;
        break;
      }
      case 1: {
        // Narrowed GLSL path lowers f64 literals through f32 payloads.
        stack[sp++] = int(uint(1069547520u));
        pc = 2;
        break;
      }
      case 2: {
        // Narrowed GLSL path lowers f64 equality compare through f32 payloads.
        float right = intBitsToFloat(stack[--sp]);
        float left = intBitsToFloat(stack[--sp]);
        stack[sp++] = (left == right) ? 1 : 0;
        pc = 3;
        break;
      }
      case 3: {
        return stack[--sp];
      }
      default: {
        return 0;
      }
    }
  }
}
void main() {
  int stack[1024];
  int sp = 0;
  ps_output.value = ps_entry_0(stack, sp);
}
