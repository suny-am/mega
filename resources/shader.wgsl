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

struct LightingUniforms {
    directions: array<vec3f, 2>,
    colors: array<vec4f, 2>,
}

@group(0) @binding(0) var<uniform> uSharedUniforms: SharedUniforms;
@group(0) @binding(1) var baseColorTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var<uniform> uLighting: LightingUniforms;

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
    let normal = normalize(in.normal);
    var shading = vec3f(0.0);
    for (var i: i32 = 0; i < 2; i++) {
        let direction = normalize(uLighting.directions[i].xyz);
        let color = uLighting.colors[i].rgb;
        shading += max(0.0, dot(direction, normal)) * color;
    }


    let baseColor = textureSample(baseColorTexture, textureSampler, in.uv).rgb;
    let color = baseColor * shading;
    let linear_color = pow(color, vec3f(2.2)); // correct color space
    return vec4f(linear_color, uSharedUniforms.color.a);
}