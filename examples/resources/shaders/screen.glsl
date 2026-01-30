uniform float u_time;

void fragment()
{
    vec2 uv = TEXCOORD * 2.0 - 1.0;

    float wave = sin(uv.y * 10.0 + u_time * 3.0) * 0.02;
    uv.x += wave;
    uv.y += sin(uv.x * 10.0 + u_time * 2.0) * 0.02;

    vec3 color = SampleColor(uv * 0.5 + 0.5);

    float vignette = 1.0 - dot(uv, uv) * 0.25;
    color *= vignette;

    COLOR = color;
}
