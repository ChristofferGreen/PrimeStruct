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
        stack[sp++] = 41;
        pc = 1;
        break;
      }
      case 1: {
        locals[3] = stack[--sp];
        pc = 2;
        break;
      }
      case 2: {
        // GLSL backend lowers local addresses to deterministic slot-byte offsets.
        stack[sp++] = 24;
        pc = 3;
        break;
      }
      case 3: {
        // GLSL backend loads locals through deterministic aligned byte-slot addressing.
        uint loadIndirectAddress = uint(stack[--sp]);
        int loadIndirectValue = 0;
        if ((loadIndirectAddress & 7u) == 0u) {
          uint loadIndirectIndex = loadIndirectAddress >> 3u;
          if (loadIndirectIndex <= 1023u) {
            loadIndirectValue = locals[loadIndirectIndex];
          }
        }
        stack[sp++] = loadIndirectValue;
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
