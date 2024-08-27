struct VertexInput {
	@location(0) position: vec3f, // 3 dimensions
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
    let ratio = 640.0 / 480.0;
    let time = uSharedUniforms.time;
    let angle = uSharedUniforms.time;
    let alpha = cos(angle);
    let beta = sin(angle);
    var position = vec3f(
        in.position.x,
        alpha * in.position.y + beta * in.position.z,
        alpha * in.position.z - beta * in.position.y,
    );
    out.position = vec4f(position.x, position.y * ratio, 0.0, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = in.color * uSharedUniforms.color.rgb;
    let linear_color = pow(color, vec3f(2.2)); // correct color space
    return vec4f(linear_color, uSharedUniforms.color.a);
}