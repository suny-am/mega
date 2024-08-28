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

const pi = 3.14159265359;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let ratio = 640.0 / 480.0;
    
    // Scaling matrix
    let S = transpose(mat4x4f(
        0.3, 0.0, 0.0, 0.0,
        0.0, 0.3, 0.0, 0.0,
        0.0, 0.0, 0.3, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ));

    // Translation matrix
    let T = transpose(mat4x4f(
        1.0, 0.0, 0.0, 0.5,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ));


    let angle1 = uSharedUniforms.time;
    let c1 = cos(angle1);
    let s1 = sin(angle1);
    let R1 = transpose(mat4x4f(
        c1, s1, 0.0, 0.0,
        -s1, c1, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ));

    let angle2 = 3.0 * pi / 4.0;
    let c2 = cos(angle2);
    let s2 = sin(angle2);
    let R2 = transpose(mat4x4f(
        1.0, 0.0, 0.0, 0.0,
        0.0, c2, s2, 0.0,
        0.0, -s2, c2, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ));

    let homogenous_position = vec4f(in.position, 1.0);
    let position = (R2 * R1 * T * S * homogenous_position).xyz;

    let depth = position.z * 0.5 + 0.5; // placeholder
    out.position = vec4f(position.x, position.y * ratio, depth, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = in.color * uSharedUniforms.color.rgb;
    let linear_color = pow(color, vec3f(2.2)); // correct color space
    return vec4f(linear_color, uSharedUniforms.color.a);
}