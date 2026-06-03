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
        stack[sp++] = 24;
        pc = 1;
        break;
      }
      case 1: {
        stack[sp++] = 41;
        pc = 2;
        break;
      }
      case 2: {
        // GLSL backend stores locals through deterministic aligned byte-slot addressing.
        int storeIndirectValue = stack[--sp];
        uint storeIndirectAddress = uint(stack[--sp]);
        if ((storeIndirectAddress & 7u) == 0u) {
          uint storeIndirectIndex = storeIndirectAddress >> 3u;
          if (storeIndirectIndex <= 1023u) {
            locals[storeIndirectIndex] = storeIndirectValue;
          }
        }
        stack[sp++] = storeIndirectValue;
        pc = 3;
        break;
      }
      case 3: {
        stack[sp++] = locals[3];
        pc = 4;
        break;
      }
      case 4: {
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
