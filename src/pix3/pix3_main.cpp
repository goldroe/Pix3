#include "IconsFontAwesome5.h"
#include <ft2build.h>
#include <freetype/freetype.h>

#pragma warning(push)
#pragma warning( disable : 4244)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma warning(pop)

#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_DECORATE(name) hoc_##name
#include <stb_sprintf.h>

#include "base/base_core.h"
#include "base/base_arena.h"
#include "base/base_math.h"
#include "base/base_strings.h"
#include "auto_array.h"
#include "os/os.h"
#include "path/path.h"
#include "lexer/lexer.h"
#include "render/render_core.h"
#include "render/d3d11/render_d3d11.h"
#include "font/font.h"
#include "ui/ui_core.h"
#include "ui/ui_widgets.h"
#include "draw/draw.h"
#include "pix3/pix3.h"

#include "base/base_core.cpp"
#include "base/base_arena.cpp"
#include "base/base_math.cpp"
#include "base/base_strings.cpp"
#include "os/os.cpp"
#include "path/path.cpp"
#include "lexer/lexer.cpp"
#include "render/render_core.cpp"
#include "render/d3d11/render_d3d11.cpp"
#include "font/font.cpp"
#include "draw/draw.cpp"
#include "ui/ui_core.cpp"
#include "ui/ui_widgets.cpp"
#include "pix3/pix3.cpp"

bool window_should_close;

int main(int argc, char **argv) {
    int target_frames_per_second = 60;
    int target_ms_per_frame = (int)(1000.f / (f32)target_frames_per_second);

    QueryPerformanceFrequency((LARGE_INTEGER *)&win32_performance_frequency);
    timeBeginPeriod(1);

    win32_event_arena = make_arena(get_malloc_allocator());

#define CLASSNAME "PIX3_WINDOW_CLASS"
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WNDCLASSA window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = win32_proc;
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = CLASSNAME;
    window_class.hInstance = hinstance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    if (!RegisterClassA(&window_class)) {
        printf("RegisterClassA failed, err:%d\n", GetLastError());
    }

    HWND hWnd = CreateWindowExA(WS_EX_ACCEPTFILES, CLASSNAME, "PIX3", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hinstance, NULL);
    DragAcceptFiles(hWnd, TRUE);

    OS_Handle window_handle = (OS_Handle)hWnd;

    d3d11_render_initialize(hWnd);

    Arena *font_arena = make_arena(get_malloc_allocator());
    String8 exe_path = os_exe_path(font_arena);
    String8 font_path = str8_concat(font_arena, exe_path, str8_lit("data/fonts/SegUI.ttf"));
    default_fonts[FONT_DEFAULT] = load_font(font_arena, font_path, 16);

    // font_path = str8_concat(font_arena, exe_path, str8_lit("data/fonts/" FONT_ICON_FILE_NAME_FAR));
    // default_fonts[FONT_ICON] = load_font_range(font_arena, font_path, 16, icons_range);
    ui_set_state(ui_state_new());

    {
        Arena *arena = make_arena(get_malloc_allocator());
        g_app_state = push_array(arena, App_State, 1);
        g_app_state->arena = arena;
        g_app_state->fs_state = push_array(arena, File_System_State, 1);
    }

    // String8 file_name = str8_zero();
    // String8 directory = str8_zero();
    // //@Note Load initial files
    // Arena *scratch = make_arena(get_malloc_allocator());
    // for (int i = 1; i < argc; i++) {
    //     char *arg = argv[i];
    //     String8 argument = str8_cstring(arg);
    //     //@Todo Options
    //     if (arg[0] == '-') {
    //     } else {
    //         file_name = argument;
    //         directory = path_strip_dir_name(scratch, argument);
    //         asset_load(g_app_state, argument);
    //     }
    // }

    // if (!directory.data) {
    //     directory = os_current_dir(g_app_state->arena);
    // }

    // load_directory_files(file_name, directory);
    // register_directory_files(file_name, directory);

    v2 old_window_dim = V2();
    
    f32 dt = 0.0f;
    s64 start_clock, last_clock;
    start_clock = last_clock = get_wall_clock(); 
 
    while (!window_should_close) {
        MSG message;
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                window_should_close = true;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        
        v2 window_dim = os_get_window_dim(window_handle);
        if (window_dim != old_window_dim) {
            //@Note Do not resize if window is minimized
            if (window_dim.x != 0 && window_dim.y != 0) {
                d3d11_resize_render_target_view((UINT)window_dim.x, (UINT)window_dim.y);
                old_window_dim = window_dim;
            }
        }

        Rect draw_region = make_rect(0.f, 0.f, window_dim.x, window_dim.y);
        r_d3d11_state->draw_region = draw_region;

        update_and_render(&win32_events, window_handle, dt / 1000.f);

        win32_events.first = nullptr;
        win32_events.last = nullptr;
        win32_events.count = 0;
        arena_clear(win32_event_arena);

        s64 work_clock = get_wall_clock();
        f32 work_ms_elapsed = get_ms_elapsed(last_clock, work_clock);
        if ((int)work_ms_elapsed < target_ms_per_frame) {
            DWORD sleep_ms = target_ms_per_frame - (int)work_ms_elapsed;
            Sleep(sleep_ms);
        }

        s64 end_clock = get_wall_clock();
        dt = get_ms_elapsed(last_clock, end_clock);
#if 0
        printf("DT: %f\n", dt);
#endif
        last_clock = end_clock;
    }

    return 0;
}
