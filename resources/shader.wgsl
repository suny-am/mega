struct VertexInput {
  @location(0) position: vec3f,
  @location(1) color: vec3f,
}

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) color: vec3f,
}

struct MyUniforms {
    color: vec4f,
    time: f32,
}

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
  var out: VertexOutput;
  let ratio = 640.0 / 480.0;

  var offset = vec2f(-0.687, -0.463);
  offset += 0.3 * vec2f(cos(uMyUniforms.time), sin(uMyUniforms.time));

  let angle = uMyUniforms.time;
  let alpha: f32 = cos(angle);
  let beta: f32 = sin(angle);
  var position = vec3f(
    in.position.x,
    alpha * in.position.y + beta * in.position.z,
    alpha * in.position.z - beta * in.position.y,
  );

  out.position = vec4f(position.x, position.y * ratio, position.z * 0.5 + 0.5, 1.0);
  out.color = in.color;
  return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
  let color = in.color * uMyUniforms.color.rgb;

  let linear_color = pow(color, vec3f(2.2));
  return vec4f(linear_color, uMyUniforms.color.a);
}
