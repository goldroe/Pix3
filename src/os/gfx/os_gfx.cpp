global Arena *os_window_arena;

global f32 g_custom_title_bar_thickness;
global OS_Area_Node *g_custom_title_bar_client_area_node;

#ifdef OS_WINDOWS
#include "win32/os_gfx_win32.cpp"
#endif

internal void os_push_custom_title_bar_client_area(Rect rect) {
    OS_Area_Node *node = push_array(os_window_arena, OS_Area_Node, 1);
    node->rect = rect;
    SLLStackPush(g_custom_title_bar_client_area_node, node);
}

internal void os_set_custom_title_bar(f32 thickness) {
    g_custom_title_bar_thickness = thickness;
}
