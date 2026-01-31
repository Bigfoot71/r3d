#pragma usage opaque shadow

uniform sampler2D u_texture;
uniform float u_time;

flat varying float v_time;

void vertex() {
    v_time = 0.5 * sin(u_time) + 0.5;
    POSITION *= 1.0 + v_time;
}

void fragment() {
    vec2 uv = TEXCOORD;
    uv.y += v_time * 0.5;

    const vec3 base_color = vec3(0.5, 0.1, 0.0);
    const vec3 active_color = vec3(1.0, 0.2, 0.0);
    vec3 color = mix(base_color, active_color, v_time);

    ALBEDO = color * texture(u_texture, uv).rgb;
    EMISSION = ALBEDO * v_time * 0.5;

    ROUGHNESS = mix(1.0, 0.25, v_time);
    METALNESS = mix(0.5, 1.0, v_time);
}
