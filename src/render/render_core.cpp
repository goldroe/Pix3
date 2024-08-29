
global R_2D_Vertex g_r_2d_vertex_nil;

internal R_2D_Vertex r_2d_vertex(f32 x, f32 y, f32 u, f32 v, v4 color) {
    R_2D_Vertex result = g_r_2d_vertex_nil;
    result.dst.x = x;
    result.dst.y = y;
    result.src.x = u;
    result.src.y = v;
    result.color = color;
    return result;
}
