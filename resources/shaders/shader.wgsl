const PI = 3.14159265359;

// /* **************** SHADING **************** */

// /**
//  * Standard properties of a material
//  * (Describe all the properties of a surface needed to shade it)
//  */
struct MaterialProperties {
	baseColor: vec3f,
	roughness: f32,
	metallic: f32,
	reflectance: f32,
	highQuality: u32, // bool, turn on costly extra visual effects
}

// /**
//  * Given some material properties, the BRDF (Bidirectional Reflection
//  * Distribution Function) tells the probability that a photon coming from an
//  * incoming direction (towards the light) is reflected in an outgoing direction
//  * (towards the camera).
//  * The returned value gives a different probability for each color channel.
//  */

fn brdf(
    material: MaterialProperties,
    normal: vec3f, // assumed to be normalized
    incomingDirection: vec3f, // assumed to be normalized
    outgoingDirection: vec3f, // assumed to be normalized
) -> vec3f {
	// Switch to compact notations used in math formulas
	// (notations of https://google.github.io/filament/Filament.html)
    let L = incomingDirection;
    let V = outgoingDirection;
    let N = normal;
    let H = normalize(L + V);
    let alpha = material.roughness * material.roughness;

    let NoV = abs(dot(N, V)) + 1e-5;
    let NoL = clamp(dot(N, L), 0.0, 1.0);
    let NoH = clamp(dot(N, H), 0.0, 1.0);
    let LoH = clamp(dot(L, H), 0.0, 1.0);

	// == Specular (reflected) lobe ==
	// Contribution of the Normal Distribution Function (NDF)
    let D = D_GGX(NoH, alpha);
	// Self-shadowing
    let Vis = V_SmithGGXCorrelatedFast(NoV, NoL, alpha);
	// Fresnel
    let f0_dielectric = vec3f(0.16 * material.reflectance * material.reflectance);
    let f0_conductor = material.baseColor;
    let f0 = mix(f0_dielectric, f0_conductor, material.metallic);
    let F = F_Schlick_vec3f(LoH, f0, 1.0);
    let f_r = D * Vis * F;

	// == Diffuse lobe ==
    let diffuseColor = (1.0 - material.metallic) * material.baseColor;
    var f_d = vec3f(0.0);
    if material.highQuality != 0u {
        f_d = diffuseColor * Fd_Burley(NoV, NoL, LoH, alpha);
    } else {
        f_d = diffuseColor * Fd_Lambert();
    }

    return f_r + f_d;
}

fn D_GGX(NoH: f32, roughness: f32) -> f32 {
    let a = NoH * roughness;
    let k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

fn V_SmithGGXCorrelatedFast(NoV: f32, NoL: f32, roughness: f32) -> f32 {
    let a = roughness;
    let GGXV = NoL * (NoV * (1.0 - a) + a);
    let GGXL = NoV * (NoL * (1.0 - a) + a);
    return clamp(0.5 / (GGXV + GGXL), 0.0, 1.0);
}

// f90 is 1.0 for specular
fn F_Schlick_vec3f(u: f32, f0: vec3f, f90: f32) -> vec3f {
    let v_pow_1 = 1.0 - u;
    let v_pow_2 = v_pow_1 * v_pow_1;
    let v_pow_5 = v_pow_2 * v_pow_2 * v_pow_1;
    return f0 * (1.0 - v_pow_5) + vec3f(f90) * v_pow_5;
}
fn F_Schlick_f32(u: f32, f0: f32, f90: f32) -> f32 {
    let v_pow_1 = 1.0 - u;
    let v_pow_2 = v_pow_1 * v_pow_1;
    let v_pow_5 = v_pow_2 * v_pow_2 * v_pow_1;
    return f0 * (1.0 - v_pow_5) + f90 * v_pow_5;
}

fn Fd_Lambert() -> f32 {
    return 1.0 / PI;
}

// More costly but more realistic at grazing angles
fn Fd_Burley(NoV: f32, NoL: f32, LoH: f32, roughness: f32) -> f32 {
    let f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    let lightScatter = F_Schlick_f32(NoL, 1.0, f90);
    let viewScatter = F_Schlick_f32(NoV, 1.0, f90);
    return lightScatter * viewScatter / PI;
}

// /*
// /**
//  * Sample a local normal from the normal map and rotate it using the normal
//  * frame to get a global normal.
//  */
// fn sampleNormal(in: VertexOutput, normalMapStrength: f32) -> vec3f {
// 	let encodedN = textureSample(normalTexture, normalSampler, in.uv).rgb;
// 	let localN = encodedN - 0.5;
// 	let rotation = mat3x3f(
// 		normalize(in.tangent),
// 		normalize(in.bitangent),
// 		normalize(in.normal),
// 	);
// 	let rotatedN = normalize(rotation * localN);
// 	return normalize(mix(in.normal, rotatedN, normalMapStrength));
// }
// */

// /* **************** UTILITIES **************** */

// check that the provided baseColor is not vec3f(0.0), i.e. "colorless"
fn validateColor(color: vec3f) -> bool {
    return all(color != vec3f(0.0)); 
}


// /* **************** BINDINGS **************** */

struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) viewDirection: vec3f,
};

// /**
//  * A structure holding the value of our uniforms
//  */
struct GlobalUniforms {
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
	cameraWorldPosition: vec3f,
	worldColor: vec3f,
	time: f32,
	gamma: f32,
};

// /**
//  * A structure holding the lighting settings
//  */
struct LightingUniforms {
	directions: array<vec4f, 2>,
	colors: array<vec4f, 2>,
}

// /**
//  * Uniforms specific to a given GLTF node.
//  */
struct NodeUniforms {
	modelMatrix: mat4x4f,
}

// /**
//  * A structure holding material properties as they are provided from the CPU code
//  */
struct MaterialUniforms {
	baseColorFactor: vec4f,
	metallicFactor: f32,
	roughnessFactor: f32,
	baseColorTexCoords: u32,
	metallicTexCoords: u32,
	roughnessTexCoords: u32,
}

// General bind group
@group(0) @binding(0) var<uniform> uGlobal: GlobalUniforms;
@group(0) @binding(1) var<uniform> uLighting: LightingUniforms;

// Material bind group
@group(1) @binding(0) var<uniform> uMaterial: MaterialUniforms;
@group(1) @binding(1) var baseColorTexture: texture_2d<f32>;
@group(1) @binding(2) var baseColorSampler: sampler;
@group(1) @binding(3) var metallicRoughnessTexture: texture_2d<f32>;
@group(1) @binding(4) var metallicRoughnessSampler: sampler;
@group(1) @binding(5) var normalTexture: texture_2d<f32>;
@group(1) @binding(6) var normalSampler: sampler;

// Node bind group
@group(2) @binding(0) var<uniform> uNode: NodeUniforms;

// /* **************** VERTEX MAIN **************** */

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let worldPosition = uNode.modelMatrix * vec4f(in.position, 1.0) * uGlobal.modelMatrix;
    out.position = uGlobal.projectionMatrix * uGlobal.viewMatrix * worldPosition;
    out.normal = (uNode.modelMatrix * vec4f(in.normal, 0.0)).xyz;
    out.color = in.color;
    out.uv = in.uv;
    out.viewDirection = uGlobal.cameraWorldPosition - worldPosition.xyz;
    return out;
}

// /* **************** FRAGMENT MAIN **************** */

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	// Sample texture
    var baseColor = textureSample(baseColorTexture, baseColorSampler, in.uv).rgb;

    let noTextureVecDefault = vec3f(0.0);

    if !validateColor(baseColor) {
        baseColor = uMaterial.baseColorFactor.rgb;
    }

    let metallicRoughness = textureSample(metallicRoughnessTexture, metallicRoughnessSampler, in.uv).rgb;

    let material = MaterialProperties(
        baseColor,
        metallicRoughness.y,
        metallicRoughness.x,
        1.0, // reflectance
        1u, // high quality
    );

    let normalMapStrength = 1.0;
	//let N = sampleNormal(in, normalMapStrength);
    let N = normalize(in.normal);
    let V = normalize(in.viewDirection);

	// Compute shading
    var color = vec3f(0.0);
    for (var i: i32 = 0; i < 2; i++) {
        let L = normalize(uLighting.directions[i].xyz);
        let lightEnergy = uLighting.colors[i].rgb;
        color += brdf(material, N, L, V) * lightEnergy;
    }

	// Debug normals
	//color = N * 0.5 + 0.5;
	
	// Gamma-correction
    let corrected_color = pow(color, vec3f(uGlobal.gamma));
    return vec4f(corrected_color, 1.0);
}