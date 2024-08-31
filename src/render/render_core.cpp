global R_2D_Vertex g_r_2d_vertex_nil;
global R_2D_Rect g_r_2d_rect_nil;

internal R_2D_Vertex r_2d_vertex(f32 x, f32 y, f32 u, f32 v, v4 color) {
    R_2D_Vertex result = g_r_2d_vertex_nil;
    result.dst.x = x;
    result.dst.y = y;
    result.src.x = u;
    result.src.y = v;
    result.color = color;
    return result;
}

internal R_2D_Rect r_2d_rect(Rect dst, Rect src, v4 color, f32 border_thickness, f32 omit_tex) {
    R_2D_Rect result = g_r_2d_rect_nil;
    result.dst = dst;
    result.src = src;
    result.color = color;
    result.border_thickness = border_thickness;
    result.omit_tex = omit_tex;
    return result;
}
