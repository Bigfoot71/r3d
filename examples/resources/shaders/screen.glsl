uniform float u_time;

void fragment() {
    vec2 uv = TEXCOORD * 2.0 - 1.0;
    uv.x += sin(uv.y * 10.0 + u_time * 3.0) * 0.01;
    uv.y += sin(uv.x * 10.0 + u_time * 2.0) * 0.01;
    COLOR = SampleColor(uv * 0.5 + 0.5);
}
