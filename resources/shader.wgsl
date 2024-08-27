struct VertexInput {
	@location(0) position: vec2f,
	@location(1) color: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
};


struct SharedUniforms {
    color: vec4f,
    time: f32,

}
@group(0) @binding(0) var<uniform> uSharedUniforms: SharedUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    var offset = vec2f(-0.6875, -0.463);
    let ratio = 640.0 / 480.0;
    let time = uSharedUniforms.time;
    offset += 0.3 * vec2f(cos(time), sin(time));
    out.position = vec4f(in.position.x + offset.x, (in.position.y + offset.y) * ratio, 0.0, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = in.color * uSharedUniforms.color.rgb;
    let linear_color = pow(color, vec3f(2.2)); // correct color space
    return vec4f(linear_color, uSharedUniforms.color.a);
}