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
        stack[sp++] = int(uint(1086324736u));
        pc = 1;
        break;
      }
      case 1: {
        // Narrowed GLSL path keeps f32/f64 conversion as bit-preserving passthrough.
        pc = 2;
        break;
      }
      case 2: {
        // Narrowed GLSL path lowers f64/i64 conversion through f32 payloads.
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
        pc = 3;
        break;
      }
      case 3: {
        --sp;
        pc = 4;
        break;
      }
      case 4: {
        stack[sp++] = int(uint(1091567616u));
        pc = 5;
        break;
      }
      case 5: {
        // Narrowed GLSL path keeps f32/f64 conversion as bit-preserving passthrough.
        pc = 6;
        break;
      }
      case 6: {
        // Narrowed GLSL path lowers f64/u64 conversion through f32 payloads.
        float value = intBitsToFloat(stack[--sp]);
        uint converted = 0u;
        if (isnan(value) || value <= 0.0) {
          converted = 0u;
        } else if (value >= 2147483647.0) {
          converted = 2147483647u;
        } else {
          converted = uint(int(value));
        }
        stack[sp++] = int(converted);
        pc = 7;
        break;
      }
      case 7: {
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
