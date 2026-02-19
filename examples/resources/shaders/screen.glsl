uniform float u_time_scale;

void fragment() {
    vec2 uv = TEXCOORD * 2.0 - 1.0;
    float time = TIME * u_time_scale;
    uv.x += sin(uv.y * 10.0 + time * 3.0) * 0.01;
    uv.y += sin(uv.x * 10.0 + time * 2.0) * 0.01;
    COLOR = SampleColor(uv * 0.5 + 0.5);
}
