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
        stack[sp++] = 8;
        pc = 1;
        break;
      }
      case 1: {
        // Narrowed GLSL path lowers i64/f64 conversion through f32 payloads.
        stack[sp++] = floatBitsToInt(float(stack[--sp]));
        pc = 2;
        break;
      }
      case 2: {
        --sp;
        pc = 3;
        break;
      }
      case 3: {
        stack[sp++] = 9;
        pc = 4;
        break;
      }
      case 4: {
        // Narrowed GLSL path lowers u64/f64 conversion through f32 payloads.
        stack[sp++] = floatBitsToInt(float(uint(stack[--sp])));
        pc = 5;
        break;
      }
      case 5: {
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
