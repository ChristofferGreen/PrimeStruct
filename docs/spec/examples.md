# Examples (sketch)

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **examples**.

## Examples (sketch)
```
// Hello world (native/VM-friendly)
[return<i32> effects(io_out)]
main() {
  print_line("Hello, world!"utf8)
  return(0i32)
}

// Pull std::io at version 1.2.0
import<"/std/io", version="1.2.0">

import /std/math/*

[operators return<i32>] add<i32>(a, b) { return(plus(a, b)) }

[operators] execute_add<i32>(x, y)

clamp_exposure(img) {
  if(
    greater_than(img.exposure, 1.0f32),
    then() { print_line_error("exposure too high"utf8) },
    else() { }
  )
}

tweak_color([copy mut restrict<Image>] img) {
  assign(img.exposure, clamp(img.exposure, 0.0f32, 1.0f32))
  assign(img.gamma, plus(img.gamma, 0.1f32))
  apply_grade(img)
}

restrict_demo() {
  [restrict<array<i32>>] ok{array<i32>{1i32, 2i32}}
  [restrict<array<i32>>] bad{array<u64>{1u64, 2u64}} // diagnostic: expected array<i32>
}

// Canonicalization example for parameters:
// Surface: f1([i32] a [f32] b) { ... }
// Canonical: f1([restrict<i32>] a [restrict<f32>] b) { ... }

[return<i32>]
convert_demo() {
  return(i32{1.5f32})
}

[return<i32>]
labeled_args_demo() {
  return(sum3(1i32 [c] 3i32 [b] 2i32))
}

[operators return<f32>]
blend([f32] a, [f32] b) {
  [f32 mut] result{(a + b) * 0.5f32}
  if (result > 1.0f32) {
    result = 1.0f32
  } else {
  }
  return(result)
}

// Compute shader sketch (GPU backend)
[compute workgroup_size(64, 1, 1)]
/add_one(
  [Buffer<i32>] input,
  [Buffer<i32>] output,
  [i32] count
) {
  [i32] x{ /std/gpu/global_id_x() }
  if (x >= count) {
    return()
  } else {
  }
  [i32] value{ /std/gpu/buffer_load(input, x) }
  /std/gpu/buffer_store(output, x, plus(value, 1i32))
}

[effects(gpu_dispatch) return<i32>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32, 3i32, 4i32}}
  [Buffer<i32>] input{ /std/gpu/upload(values) }
  [Buffer<i32>] output{ /std/gpu/buffer<i32>(4i32) }
  /std/gpu/dispatch(/add_one, 4i32, 1i32, 1i32, input, output, 4i32)
  [array<i32>] result{ /std/gpu/readback(output) }
  return(plus(plus(result[0i32], result[1i32]), plus(result[2i32], result[3i32])))
}

// Canonical, post-transform form
[return<f32>] blend([f32] a, [f32] b) {
  [f32 mut] result{multiply(plus(a, b), 0.5f32)}
  if(
    greater_than(result, 1.0f32),
    then() { assign(result, 1.0f32) },
    else() { }
  )
  return(result)
}

// IR sketch
module {
  def /convert_demo(): i32 {
    return i32{1.5f32}
  }
  def /labeled_args_demo(): i32 {
    return sum3(1, [c] 3, [b] 2)
  }
}
```
