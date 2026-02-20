uniform vec3 u_color;
uniform ivec2 u_cells;
uniform float u_line_px;

void fragment()
{
    vec2 uv = TEXCOORD;

    vec2 cells = max(vec2(u_cells), vec2(1.0));
    vec2 g = uv * cells;
    vec2 cell_uv = fract(g);
    vec2 dist_to_edge = min(cell_uv, 1.0 - cell_uv);

    vec2 px = fwidth(cell_uv);
    vec2 half_thick = 0.5 * u_line_px * px;

    float line_x = 1.0 - smoothstep(half_thick.x, half_thick.x + px.x, dist_to_edge.x);
    float line_y = 1.0 - smoothstep(half_thick.y, half_thick.y + px.y, dist_to_edge.y);

    float grid = max(line_x, line_y);

    COLOR = u_color * grid;
}
