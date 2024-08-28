struct VertexInput {
	@location(0) position: vec3f, // 3 dimensions
	@location(1) color: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
};


struct SharedUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
    time: f32,

}

@group(0) @binding(0) var<uniform> uSharedUniforms: SharedUniforms;

const pi = 3.14159265359;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uSharedUniforms.projectionMatrix * uSharedUniforms.viewMatrix * uSharedUniforms.modelMatrix * vec4f(in.position, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = in.color * uSharedUniforms.color.rgb;
    let linear_color = pow(color, vec3f(2.2)); // correct color space
    return vec4f(linear_color, uSharedUniforms.color.a);
}