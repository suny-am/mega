struct VertexInput {
	@location(0) position: vec3f, // position.xyz
    @location(1) normal: vec3f,// normal.xyz
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
	@location(4) tangent: vec3f,
	@location(5) bitangent: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) viewDirection: vec3f,
	@location(4) tangent: vec3f,
	@location(5) bitangent: vec3f,
};

struct SharedUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
    cameraWorldPosition: vec3f,
    time: f32,
}

struct LightingUniforms {
    directions: array<vec3f, 2>,
    colors: array<vec4f, 2>,
    hardness: f32,
    kd: f32,
    ks: f32,
    kn: f32,
}

@group(0) @binding(0) var<uniform> uSharedUniforms: SharedUniforms;
@group(0) @binding(1) var baseColorTexture: texture_2d<f32>;
@group(0) @binding(2) var normalTexture: texture_2d<f32>;
@group(0) @binding(3) var textureSampler: sampler;
@group(0) @binding(4) var<uniform> uLighting: LightingUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let worldPosition = uSharedUniforms.modelMatrix * vec4f(in.position, 1.0);
    out.position = uSharedUniforms.projectionMatrix * uSharedUniforms.viewMatrix * worldPosition;
    out.tangent = (uSharedUniforms.modelMatrix * vec4(in.tangent, 0.0)).xyz;
    out.bitangent = (uSharedUniforms.modelMatrix * vec4(in.bitangent, 0.0)).xyz;
    out.normal = (uSharedUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
    out.color = in.color;
    out.uv = in.uv;
    out.viewDirection = uSharedUniforms.cameraWorldPosition - worldPosition.xyz;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let encodedN = textureSample(normalTexture, textureSampler, in.uv).rgb;
    let localN = encodedN * 2.0 - 1.0;

    let localToWorld = mat3x3f(
        normalize(in.tangent),
        normalize(in.bitangent),
        normalize(in.normal),
    );
    let worldN = localToWorld * localN;
    let normalMapStrength = uLighting.kn;
    let N = mix(in.normal, worldN, normalMapStrength);

    let V = normalize(in.viewDirection);

    let baseColor = textureSample(baseColorTexture, textureSampler, in.uv).rgb;
    let kd = uLighting.kd;
    let ks = uLighting.ks;
    let hardness = uLighting.hardness;

    var color = vec3f(0.0);
    for (var i: i32 = 0; i < 2; i++) {
        let lightColor = uLighting.colors[i].rgb;
        let L = normalize(uLighting.directions[i].xyz);
        let R = reflect(-L, N); // shorthand for (2.0 * dot(N,L) * N - L)

        let diffuse = max(0.0, dot(L, N)) * lightColor;

        let RoV = max(0.0, dot(R, V));
        let specular = pow(RoV, hardness);

        color += baseColor * kd * diffuse + ks * specular;
    }

    let linear_color = pow(color, vec3f(2.2)); // correct color space
    return vec4f(linear_color, uSharedUniforms.color.a);
}