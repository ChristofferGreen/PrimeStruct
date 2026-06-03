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
        stack[sp++] = int(uint(1065353216u));
        pc = 1;
        break;
      }
      case 1: {
        // Narrowed GLSL path keeps f32/f64 conversion as bit-preserving passthrough.
        pc = 2;
        break;
      }
      case 2: {
        // Narrowed GLSL path keeps f64/f32 conversion as bit-preserving passthrough.
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
