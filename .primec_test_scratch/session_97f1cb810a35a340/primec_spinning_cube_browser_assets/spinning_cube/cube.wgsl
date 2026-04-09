struct VertexIn {
  @location(0) position : vec4<f32>,
  @location(1) color : vec4<f32>,
};

struct VertexOut {
  @builtin(position) clipPosition : vec4<f32>,
  @location(0) color : vec4<f32>,
};

@vertex
fn vsMain(in : VertexIn) -> VertexOut {
  var out : VertexOut;
  out.clipPosition = in.position;
  out.color = in.color;
  return out;
}

@fragment
fn fsMain(in : VertexOut) -> @location(0) vec4<f32> {
  return in.color;
}
