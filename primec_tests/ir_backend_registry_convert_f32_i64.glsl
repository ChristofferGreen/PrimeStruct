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
        stack[sp++] = int(uint(1092616192u));
        pc = 1;
        break;
      }
      case 1: {
        float value = intBitsToFloat(stack[--sp]);
        int converted = 0;
        if (isnan(value)) {
          converted = 0;
        } else if (value >= 2147483647.0) {
          converted = 2147483647;
        } else if (value <= -2147483648.0) {
          converted = -2147483647 - 1;
        } else {
          converted = int(value);
        }
        stack[sp++] = converted;
        pc = 2;
        break;
      }
      case 2: {
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
