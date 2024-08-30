struct VertexInput {
	@location(0) position: vec3f, // position.xyz
    @location(1) normal: vec3f,// normal.xyz
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

struct SharedUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
    time: f32,

}

@group(0) @binding(0) var<uniform> uSharedUniforms: SharedUniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uSharedUniforms.projectionMatrix * uSharedUniforms.viewMatrix * uSharedUniforms.modelMatrix * vec4f(in.position, 1.0);
    out.normal = (uSharedUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
    out.color = in.color;
    out.uv = in.uv;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = textureSample(gradientTexture, textureSampler, in.uv).rgb;
    let linear_color = pow(color, vec3f(2.2)); // correct color space
    return vec4f(linear_color, uSharedUniforms.color.a);
}