global HCURSOR win32_hcursor;
global Arena *win32_event_arena;
global OS_Event_List win32_events;
global bool win32_resizing;

internal void os_window_minimize(OS_Handle window_handle) {
    ShowWindow((HWND)window_handle, SW_MINIMIZE);
}

internal void os_window_maximize(OS_Handle window_handle) {
    if (IsZoomed((HWND)window_handle)) {
        ShowWindow((HWND)window_handle, SW_RESTORE);
    } else {
        ShowWindow((HWND)window_handle, SW_MAXIMIZE);
    }
}

internal v2 os_mouse_from_window(OS_Handle window_handle) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient((HWND)window_handle, &pt);
    v2 result;
    result.x = (f32)pt.x;
    result.y = (f32)pt.y;
    return result;
}

internal v2 os_get_window_dim(OS_Handle window_handle) {
    v2 result{};
    RECT rect;
    int width = 0, height = 0;
    if (GetClientRect((HWND)window_handle, &rect)) {
        result.x = (f32)(rect.right - rect.left);
        result.y = (f32)(rect.bottom - rect.top);
    }
    return result;
}

internal Rect os_client_rect_from_window(OS_Handle window_handle) {
    RECT client_rect;
    GetClientRect((HWND)window_handle, &client_rect);
    Rect result;
    result.x0 = (f32)client_rect.left;
    result.x1 = (f32)client_rect.right;
    result.y0 = (f32)client_rect.top;
    result.y1 = (f32)client_rect.bottom;
    return result;
}

internal OS_Event *win32_push_event(OS_Event_Kind kind) {
    OS_Event *result = push_array(win32_event_arena, OS_Event, 1);
    result->kind = kind;
    result->next = nullptr;
    result->flags = os_event_flags();
    SLLQueuePush(win32_events.first, win32_events.last, result);
    win32_events.count += 1;
    return result;
}

internal LRESULT CALLBACK win32_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    OS_Handle window_handle = (OS_Handle)hWnd;
    OS_Event *event = nullptr;
    bool release = false;
    int border_thickness = 4;

    LRESULT result = 0;
    switch (Msg) {
    case WM_MOUSEWHEEL:
    {
        event = win32_push_event(OS_EventKind_Scroll);
        int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        event->delta.y = delta;
        break;
    }

    case WM_ENTERSIZEMOVE:
    {
        win32_resizing = true;
        break;
    }
    case WM_EXITSIZEMOVE:
    {
        win32_resizing = false;
        break;
    }

    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam); 
        int y = GET_Y_LPARAM(lParam); 
        event = win32_push_event(OS_EventKind_MouseMove);
        event->pos.x = x;
        event->pos.y = y;
        break;
    }

    case WM_SETCURSOR:
    {
        Rect window_rect = os_client_rect_from_window(window_handle);
        v2 mouse = os_mouse_from_window(window_handle);
        bool over_border_x = mouse.x <= border_thickness || mouse.x >= window_rect.x1 - border_thickness;
        bool over_border_y = mouse.y <= border_thickness || mouse.y >= window_rect.y1 - border_thickness;
        bool over_border = over_border_x || over_border_y;
        if (!over_border && !win32_resizing && rect_contains(window_rect, mouse)) {
            SetCursor(win32_hcursor);
        } else {
            result = DefWindowProcA(hWnd, Msg, wParam, lParam);
        }
        break;
    }

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        release = true;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        event = win32_push_event(release ? OS_EventKind_MouseUp : OS_EventKind_MouseDown);
        switch (Msg) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
            event->key = OS_KEY_LEFTMOUSE;
            break;
        case WM_MBUTTONUP:
        case WM_MBUTTONDOWN:
            event->key = OS_KEY_MIDDLEMOUSE;
            break;
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
            event->key = OS_KEY_RIGHTMOUSE; 
            break;
        }
        int x = GET_X_LPARAM(lParam); 
        int y = GET_Y_LPARAM(lParam); 
        event->pos.x = x;
        event->pos.y = y;
        if (release) {
            ReleaseCapture();
        } else {
            SetCapture(hWnd);
        }
        break;
    }

    case WM_SYSCHAR:
    {
        // result = DefWindowProcA(hWnd, Msg, wParam, lParam);
        break;
    }
    case WM_CHAR:
    {
        u16 vk = wParam & 0x0000ffff;
        u8 c = (u8)vk;
        if (c == '\r') c = '\n';
        if (c >= 32 && c != 127) {
            event = win32_push_event(OS_EventKind_Text);
            event->text = str8_copy(win32_event_arena, str8(&c, 1));
        }
        break;
    }

    case WM_SYSKEYUP:
    case WM_KEYUP:
        release = true;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        u8 down = ((lParam >> 31) == 0);
        u8 alt_mod = (lParam & (1 << 29)) != 0;
        u32 virtual_keycode = (u32)wParam;
        if (virtual_keycode < 256) {
            event = win32_push_event(release ? OS_EventKind_Release : OS_EventKind_Press);
            event->key = os_key_from_vk(virtual_keycode);
        }

        if (wParam == VK_F4 && alt_mod) {
            PostQuitMessage(0);
        }
        break;
    }

    case WM_SIZE:
    {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        break;
    }

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        UINT len = DragQueryFileA(hDrop, 0, NULL, 0);
        String8 string = str8_zero();
        string.data = push_array(win32_event_arena, u8, len + 1);
        string.count = len;
        DragQueryFileA(hDrop, 0, (LPSTR)string.data, len + 1);
        event = win32_push_event(OS_EventKind_DropFile);
        event->text = string;
        DragFinish(hDrop);
        break;
    }

    case WM_NCHITTEST:
    {
        bool hit_left = false, hit_right = false, hit_top = false, hit_bottom = false;

        RECT window_rect;
        GetWindowRect(hWnd, &window_rect);

        POINT pos;
        pos.x = GET_X_LPARAM(lParam);
        pos.y = GET_Y_LPARAM(lParam);
        POINT client_pos = pos;
        ScreenToClient(hWnd, &client_pos);

        bool over_window = (window_rect.left <= pos.x && pos.x < window_rect.right && window_rect.top <= pos.y && pos.y < window_rect.bottom);
        bool over_title_bar = over_window && client_pos.y < g_custom_title_bar_thickness;

        RECT rect;
        GetClientRect(hWnd, &rect);

        if (rect.left <= client_pos.x && client_pos.x < rect.left + border_thickness) {
            hit_left = true;
        }
        if (rect.right - border_thickness <= client_pos.x && client_pos.x < rect.right) {
            hit_right = true; 
        }
        if (rect.top <= client_pos.y && client_pos.y < rect.top + border_thickness) {
            hit_top = true; 
        }
        if (rect.bottom - border_thickness <= client_pos.y && client_pos.y < rect.bottom) {
            hit_bottom = true;
        }

        result = HTNOWHERE;
        if (over_window) {
            result = HTCLIENT;

            if (over_title_bar) {
                result = HTCAPTION;
            }

            if (hit_left) result = HTLEFT;
            if (hit_right) result = HTRIGHT;
            if (hit_top) result = HTTOP;
            if (hit_bottom) result = HTBOTTOM;

            if (hit_left && hit_top) {
                result = HTTOPLEFT;
            }
            if (hit_right && hit_top) {
                result = HTTOPRIGHT;
            }
            if (hit_left && hit_bottom) {
                result = HTBOTTOMLEFT;
            }
            if (hit_right && hit_bottom) {
                result = HTBOTTOMRIGHT;
            }

            for (OS_Area_Node *area = g_custom_title_bar_client_area_node; area != nullptr; area = area->next) {
                if (area->rect.x0 <= client_pos.x && client_pos.x < area->rect.x1 && area->rect.y0 <= client_pos.y && client_pos.y < area->rect.y1) {
                    result = HTCLIENT;
                }
            }
        }
        break;
    }

    case WM_CREATE:
    {
        break;
    }
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcA(hWnd, Msg, wParam, lParam);
    }
    return result;
}

internal String8 os_get_clipboard_text(Arena *arena) {
    String8 result = str8_zero();
    if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(0)) {
        HANDLE handle = GetClipboardData(CF_TEXT);
        if (handle) {
            u8 *buffer = (u8 *)GlobalLock(handle);
            if (buffer) {
                result = str8_copy(arena, str8_cstring((char *)buffer));
                GlobalUnlock(handle);
            }
        }
        CloseClipboard();
    }
    return result;
}

internal void os_set_clipboard_text(String8 text) {
    if (OpenClipboard(0)) {
        HANDLE handle = GlobalAlloc(GMEM_MOVEABLE, text.count + 1);
        if (handle) {
            u8 *buffer = (u8 *)GlobalLock(handle);
            MemoryCopy(buffer, text.data, text.count);
            buffer[text.count] = 0;
            GlobalUnlock(handle);
            SetClipboardData(CF_TEXT, handle);
        }
    }
}

internal void os_set_cursor(OS_Cursor cursor) {
    local_persist HCURSOR hcursor;
    switch (cursor) {
    default:
    case OS_Cursor_Arrow:
        hcursor = LoadCursorA(NULL, IDC_ARROW);
        break;
    case OS_Cursor_Ibeam:
        hcursor = LoadCursorA(NULL, IDC_IBEAM);
        break;
    case OS_Cursor_Hand:
        hcursor = LoadCursorA(NULL, IDC_HAND);
        break;
    case OS_Cursor_SizeNS:
        hcursor = LoadCursorA(NULL, IDC_SIZENS);
        break;
    case OS_Cursor_SizeWE:
        hcursor = LoadCursorA(NULL, IDC_SIZEWE);
        break;
    }
    
    if (win32_hcursor != hcursor) {
        PostMessageA(0, WM_SETCURSOR, 0, 0);
        win32_hcursor = hcursor;
    }
}

internal bool os_window_is_focused(OS_Handle window_handle) {
    HWND active_hwnd = GetActiveWindow();
    bool result = (OS_Handle)active_hwnd == window_handle;
    return result;
}
