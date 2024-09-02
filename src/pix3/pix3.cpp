//@Note Input
enum Button_State {
    ButtonState_Down      = (1<<0),
    ButtonState_Pressed   = (1<<1),
    ButtonState_Released  = (1<<3),
};
EnumDefineFlagOperators(Button_State);

struct Input {
    v2_s32 mouse_position;
    v2_s32 scroll_delta;
    v2_s32 mouse_drag_start;
    Button_State buttons[OS_KEY_COUNT];
};

global App_State *g_app_state;
global Input g_input;

internal v2 get_mouse_drag_delta() {
    v2 result;
    result.x = (f32)(g_input.mouse_position.x - g_input.mouse_drag_start.x);
    result.y = (f32)(g_input.mouse_position.y - g_input.mouse_drag_start.y);
    return result;
}

internal bool ui_key_captured() {
    return ui_state->keyboard_captured || ui_state->focus_active_box_key != 0;
}

internal bool ui_mouse_captured() {
    return ui_state->mouse_captured || ui_state->focus_active_box_key != 0;
}

#define button_down(Button)     (!ui_key_captured() && g_input.buttons[(Button)] & ButtonState_Down)
#define button_pressed(Button)  (!ui_key_captured() && g_input.buttons[(Button)] & ButtonState_Pressed)
#define button_released(Button) (!ui_key_captured() && g_input.buttons[(Button)] & ButtonState_Released)

internal Button_State get_button_state(OS_Key key) { return g_input.buttons[key]; }
internal bool key_up(OS_Key key)      { bool result = button_released(key); return result; }
internal bool key_pressed(OS_Key key) { bool result = button_pressed(key); return result; }
internal bool key_down(OS_Key key)    { bool result = button_down(key); return result; }

internal R_Handle load_texture(String8 file_name) {
    int x, y, n;
    u8 *data = stbi_load((char *)file_name.data, &x, &y, &n, 4);
    R_Handle texture = d3d11_create_texture(R_Tex2DFormat_R8G8B8A8, v2s32(x, y), data);
    stbi_image_free(data);
    return texture;
}

internal Asset *asset_load(App_State *app_state, String8 file_name) {
    Asset *result = nullptr;
    Arena *scratch = make_arena(get_malloc_allocator());
    for (Asset *asset = app_state->assets.first; asset; asset = asset->next) {
        String8 file_path = path_join(scratch, app_state->current_directory, asset->file->file_name);
        if (str8_match(asset->file->file_name, file_name, StringMatchFlag_CaseInsensitive)) {
            int x, y, n;
            u8 *bitmap = stbi_load((char *)file_path.data, &x, &y, &n, 4);
            Assert(bitmap);
            R_Handle texture = d3d11_create_texture(R_Tex2DFormat_R8G8B8A8, v2s32(x, y), bitmap);
            stbi_image_free(bitmap);
            asset->kind = AssetKind_Image;
            asset->textures = push_array(g_app_state->arena, R_Handle, 1);
            asset->textures[0] = texture;
            asset->texture_count = 1;
            result = asset;
        }
    }
    return result;
}

struct OS_File_Node {
    OS_File_Node *next;
    OS_File file;
};

internal int fs_sort_files__default(const void *a, const void *b) {
    int result = 0;
    OS_File *arg1 = (OS_File *)a;
    OS_File *arg2 = (OS_File *)b;
    result = strcmp((char *)arg1->file_name.data, (char *)arg2->file_name.data);
    return result;
}

internal void file_system_load_files(File_System_State *fs, String8 full_path) {
    MemoryCopy(fs->path_buffer, full_path.data, full_path.count);
    fs->path_len = full_path.count;
    fs->path_pos = fs->path_len;

    String8 file_path = str8(fs->path_buffer, fs->path_len);

    if (!fs->cached_files_arena) {
        fs->cached_files_arena = make_arena(get_virtual_allocator());
    }

    arena_clear(fs->cached_files_arena);

    OS_File_Node *first = nullptr, *last = nullptr;
    int file_count = 0;

    Arena *scratch = make_arena(get_virtual_allocator());
    OS_File file;
    OS_Handle find_handle = os_find_first_file(fs->cached_files_arena, file_path, &file);
    do {
        OS_File_Node *node = push_array(scratch, OS_File_Node, 1);
        node->file = file;
        SLLQueuePush(first, last, node);
        file_count += 1;
    } while(os_find_next_file(fs->cached_files_arena, find_handle, &file));

    fs->cached_files = push_array(fs->cached_files_arena, OS_File, file_count);
    fs->cached_file_count = file_count;

    OS_File_Node *node = first;
    for (int file_idx = 0; file_idx < file_count; file_idx += 1, node = node->next) {
        fs->cached_files[file_idx] = node->file;
    }

    quick_sort(fs->cached_files, OS_File, file_count, fs_sort_files__default);

    arena_release(scratch);
}

internal void load_directory_files(App_State *app_state, String8 directory) {
    if (!app_state->asset_arena) {
        app_state->asset_arena = make_arena(get_virtual_allocator());
    }

    app_state->assets.first = app_state->assets.last = nullptr;
    app_state->assets.count = 0;
    arena_clear(app_state->asset_arena);

    Arena *scratch = make_arena(get_virtual_allocator());
    OS_File find_file;
    OS_Handle find_handle = os_find_first_file(app_state->asset_arena, directory, &find_file);
    do {
        if (str8_match(find_file.file_name, str8_lit("."), StringMatchFlag_Nil) || str8_match(find_file.file_name, str8_lit(".."), StringMatchFlag_Nil)) continue;

        String8 full_path = str8_concat(scratch, directory, find_file.file_name);
        if (stbi_info((char *)full_path.data, NULL, NULL, NULL)) {
            OS_File *file = push_array(app_state->asset_arena, OS_File, 1);
            *file = find_file;
            Asset *asset = push_array(app_state->asset_arena, Asset, 1);
            asset->file = file;

            DLLPushBack(app_state->assets.first, app_state->assets.last, asset, next, prev);
            app_state->assets.count += 1;
        }
     } while (os_find_next_file(app_state->asset_arena, find_handle, &find_file));

    // quick_sort(g_app_state->cached_files, OS_File, file_count, fs_sort_files__default);

    arena_release(scratch);

    g_app_state->current_directory = directory;
}

internal void set_current_asset(Asset *asset) {
    if (asset) {
        g_app_state->current_asset = asset;
    }
}

internal void update_and_render(OS_Event_List *events, OS_Handle window_handle, f32 dt) {
    local_persist bool first_call = true;
    if (first_call) {
        first_call = false;
        g_app_state->point_sample = true;
        g_app_state->camera.zoom = 1.f;
    }

    File_System_State *fs_state = g_app_state->fs_state;

    //@Note Input begin
    {
        for (OS_Event *evt = events->first; evt; evt = evt->next) {
            bool pressed = false;
            switch (evt->kind) {
            case OS_EventKind_DropFile:
            {
                String8 file_path = evt->text;
                String8 directory = path_strip_dir_name(arena_alloc(get_malloc_allocator(), file_path.count + 1), file_path);
                // if (path_is_relative) {
                    // full_path = path_join(arena_alloc(get_malloc_allocator(), directory.count + file_name.count + 1), directory, file_name);
                // }
                String8 file_name = path_file_name(file_path);
                load_directory_files(g_app_state, directory);
                Asset *asset = asset_load(g_app_state, file_name);
                set_current_asset(asset);
                break;
            }
            case OS_EventKind_MouseMove:
                g_input.mouse_position = evt->pos;
                break;

            case OS_EventKind_Scroll:
                g_input.scroll_delta = evt->delta;
                break;

            case OS_EventKind_MouseDown:
                g_input.mouse_drag_start = evt->pos;
                g_input.mouse_position = evt->pos;
            case OS_EventKind_Press:
                pressed = true;
            case OS_EventKind_Release:
            case OS_EventKind_MouseUp:
            {
                Button_State state = {};
                state |= (Button_State)(ButtonState_Pressed*(pressed && !button_down(evt->key)));
                state |= (Button_State)(ButtonState_Down*pressed);
                state |= (Button_State)(ButtonState_Released * (!pressed));
                g_input.buttons[evt->key] = state;
                break;
            }
            }
        }
    }

    Rect client_rect = os_client_rect_from_window(window_handle);
    v2 client_dim = rect_dim(client_rect);

    draw_begin(window_handle);

    ui_begin_build(dt * 1000.f, window_handle, events);

    //@Note Top bar
    ui_set_next_pref_width(ui_pct(1.f, 0.f));
    ui_set_next_pref_height(ui_text_dim(0.f, 1.f));
    UI_Row
        UI_PrefWidth(ui_text_dim(6.f, 1.f))
        UI_PrefHeight(ui_text_dim(0.f, 1.f))
    {
        Asset *asset = g_app_state->current_asset;
        if (asset) {
            String8 file_name = asset->file->file_name;
            ui_labelf("%s", file_name.data);
        }

        if (ui_clicked(ui_button(str8_lit("Open")))) {
            g_app_state->file_system_active = true;
            ui_text_edit_insert(fs_state->path_buffer, ArrayCount(fs_state->path_buffer), &fs_state->path_pos, &fs_state->path_len, g_app_state->current_directory);
            file_system_load_files(fs_state, str8(fs_state->path_buffer, fs_state->path_len));
        }

        UI_Signal sampler_sig = ui_buttonf("%s###sampler_opt", g_app_state->point_sample ? "Point" : "Linear");
        if (ui_clicked(sampler_sig)) {
            g_app_state->point_sample = !g_app_state->point_sample;
        }

        // UI_Signal rotate_sig = ui_button(str8_lit("Rotate"));
        // if (ui_clicked(rotate_sig)) {
        //     // g_app_state->rot += 90.f;
        //     // if (g_app_state->rot >= 360.f) g_app_state->rot = 0.f;
        // }

        String8 trash_icon_string = ui_string_from_icon_kind(UI_IconKind_Trash, "###trash");
        ui_set_next_font(default_fonts[FONT_ICON]);
        UI_Signal trash_sig = ui_button(trash_icon_string);
        if (ui_clicked(trash_sig)) {
        }
    }

    //@Note Debug information
    UI_PrefWidth(ui_text_dim(4.f, 1.f))
        UI_PrefHeight(ui_text_dim(0.f, 1.f))
    {
        // ui_labelf("mouse:%f %f", ui_mouse().x, ui_mouse().y);
        // ui_labelf("hot:%llu", ui_state->hot_box_key);
        // ui_labelf("active:%llu", ui_state->active_box_key);
        // ui_labelf("focus_active:%llu", ui_state->focus_active_box_key);
    }

    //@Note Sample column stuff
    // v2 main_bar_size = V2(520.f, 128.f);
    // Rect main_bar_rect = make_rect(client_dim.x * 0.5f - main_bar_size.x * 0.5f, client_dim.y - main_bar_size.y, main_bar_size.x, main_bar_size.y);
    // ui_set_next_fixed_x(main_bar_rect.x0);
    // ui_set_next_fixed_y(main_bar_rect.y0);
    // ui_set_next_pref_width(ui_children_sum(1.f));
    // ui_set_next_pref_height(ui_children_sum(1.f));
    // ui_set_next_child_layout_axis(Axis_X);
    // UI_Box *bottom_container = ui_make_box_from_string(UI_BoxFlag_Nil, str8_lit("###main_bottom_container"));
    // UI_Parent(bottom_container)
    // {
    //     ui_set_next_pref_width(ui_children_sum(1.f));
    //     ui_set_next_pref_height(ui_children_sum(1.f));
    //     UI_Column 
    //         UI_PrefWidth(ui_text_dim(4.f, 1.f))
    //         UI_PrefHeight(ui_text_dim(2.f, 1.f))
    //     {
    //         ui_button(str8_lit("Red: 0.1f"));
    //         ui_button(str8_lit("Blue: 0.8f"));
    //         ui_button(str8_lit("Green: 0.4f"));
    //     }
    //     ui_set_next_pref_width(ui_children_sum(1.f));
    //     ui_set_next_pref_height(ui_children_sum(1.f));
    //     UI_Column 
    //         UI_PrefWidth(ui_text_dim(4.f, 1.f))
    //         UI_PrefHeight(ui_text_dim(2.f, 1.f))
    //     {
    //         ui_button(str8_lit("Hue: 0.1f"));
    //         ui_button(str8_lit("Sat: 0.8f"));
    //         ui_button(str8_lit("Val: 0.4f"));
    //     }
    // }

    ui_spacer(Axis_Y, ui_pct(1.f, 0.f));

    //@Note Film Strip
    if (g_app_state->film_strip_active) {
        v2 img_dim = V2(96.f, 72.f);
        f32 x = 0.f;
        f32 padding = 4.f;

        ui_set_next_pref_width(ui_pct(1.f, 1.f));
        ui_set_next_pref_height(ui_children_sum(1.f));
        ui_set_next_child_layout_axis(Axis_X);
        UI_Box *film_strip_container = ui_make_box_from_string(UI_BoxFlag_Nil, str8_lit("###filmstrip"));
        UI_Parent(film_strip_container) {
            int file_idx = 0;
            for (Asset *asset = g_app_state->assets.first; asset; asset = asset->next, file_idx++) {
                ui_set_next_fixed_x(x - g_app_state->film_strip_scroll_pt.idx * img_dim.x);
                ui_set_next_fixed_y(0.f);
                ui_set_next_fixed_width(img_dim.x);
                ui_set_next_fixed_height(img_dim.y);
                ui_set_next_box_flags(UI_BoxFlag_Clickable);
                ui_set_next_border_thickness(14.f);
                UI_Signal img_sig = ui_imagef(asset->textures[0], "###img_%d", file_idx);
                if (ui_clicked(img_sig)) {
                    g_app_state->current_asset = asset;
                }
                x += img_dim.x + padding;
            }
        }

        if (g_app_state->assets.count * img_dim.x > client_dim.x) {
            ui_spacer(Axis_Y, ui_px(4.f, 1.f));
            ui_set_next_pref_width(ui_pct(1.f, 1.f));
            g_app_state->film_strip_scroll_pt = ui_scroll_bar(str8_lit("###bar"), Axis_X, ui_px(10.f, 1.f), g_app_state->film_strip_scroll_pt, rng_s64(0, (s64)(rect_width(film_strip_container->rect) / img_dim.x)), g_app_state->assets.count);
        }
    }

    ui_spacer(Axis_Y, ui_px(8.f, 1.f));

    //@Note Bottom bar
    ui_set_next_pref_width(ui_pct(1.f, 1.f));
    ui_set_next_pref_height(ui_text_dim(0.f, 1.f));
    UI_Row
        UI_PrefWidth(ui_text_dim(4.f, 1.f))
        UI_PrefHeight(ui_text_dim(0.f, 1.f))
    {
        Asset *asset = g_app_state->current_asset;
        v2_s32 size = v2s32((s32)client_dim.x, (s32)client_dim.y);
        if (asset) {
            size = r_texture_size(asset->textures[0]);
        }

        ui_set_next_font(default_fonts[FONT_ICON]);
        String8 film_string = ui_string_from_icon_kind(UI_IconKind_Images, "###film");
        if (ui_clicked(ui_button(film_string))) {
            g_app_state->film_strip_active = !g_app_state->film_strip_active;
            for (Asset *asset = g_app_state->assets.first; asset; asset = asset->next) {
                if (!asset->textures) {
                    asset_load(g_app_state, asset->file->file_name);
                }
            }
        }

        ui_spacer(Axis_X, ui_pct(1.f, 0.f));

        ui_labelf("%d%%", (int)(g_app_state->camera.zoom * 100.f));

        f32 h_r = client_dim.x / (f32)size.x;
        f32 v_r = client_dim.y / (f32)size.y;
        String8 zoom_in_string = ui_string_from_icon_kind(UI_IconKind_ZoomPlus, "###zoom_in");
        String8 zoom_out_string = ui_string_from_icon_kind(UI_IconKind_ZoomMinus, "###zoom_out");
        ui_set_next_font(default_fonts[FONT_ICON]);
        if (ui_clicked(ui_button(zoom_in_string))) {
            g_app_state->camera.zoom += .1f;
        }
        ui_set_next_font(default_fonts[FONT_ICON]);
        if (ui_clicked(ui_button(zoom_out_string))) {
            g_app_state->camera.zoom -= .1f;
        }

        if (ui_clicked(ui_button(str8_lit("Fit")))) {
            g_app_state->camera.position = V2();
            g_app_state->camera.zoom = Min(h_r, v_r);
        }
        if (ui_clicked(ui_button(str8_lit("1:1")))) {
            g_app_state->camera.position = V2();
            g_app_state->camera.zoom = 1.f;
        }
    }

    //@Note Arrows
    {
        Asset *current_asset = g_app_state->current_asset;
        if (current_asset && current_asset->prev &&
            g_input.mouse_position.x <= 100.f) {
            ui_set_next_fixed_x(10.f);
            ui_set_next_fixed_y(client_dim.y / 2.f);
            ui_set_next_pref_width(ui_text_dim(1.f, 1.f));
            ui_set_next_pref_height(ui_text_dim(1.f, 1.f));
            ui_set_next_font(default_fonts[FONT_ICON]);
            String8 left_arrow_string = ui_string_from_icon_kind(UI_IconKind_ArrowLeft, "###left_arrow");
            UI_Signal sig = ui_button(left_arrow_string);
            if (ui_clicked(sig)) {
                current_asset = current_asset->prev;
                if (!current_asset->textures) {
                    current_asset = asset_load(g_app_state, current_asset->file->file_name);
                }
                set_current_asset(current_asset);
            }
        }
        if (current_asset && current_asset->next &&
            g_input.mouse_position.x >= client_dim.x - 100.f) {
            ui_set_next_fixed_x(client_dim.x - 20.f);
            ui_set_next_fixed_y(client_dim.y / 2.f);
            ui_set_next_pref_width(ui_text_dim(1.f, 1.f));
            ui_set_next_pref_height(ui_text_dim(1.f, 1.f));
            ui_set_next_font(default_fonts[FONT_ICON]);
            String8 right_arrow_string = ui_string_from_icon_kind(UI_IconKind_ArrowRight, "###right_arrow");
            UI_Signal sig = ui_button(right_arrow_string);
            if (ui_clicked(sig)) {
                current_asset = current_asset->next;
                if (!current_asset->textures) {
                    current_asset = asset_load(g_app_state, current_asset->file->file_name);
                }
                set_current_asset(current_asset);
            }
        }
    }

    //@Note File System
    if (g_app_state->file_system_active) {
        Arena *scratch = make_arena(get_malloc_allocator());
        bool reload_files = false;

        ui_set_next_fixed_x(client_dim.x * 0.1f);
        ui_set_next_fixed_y(20);
        ui_set_next_fixed_width(client_dim.x * 0.8f);
        ui_set_next_fixed_height(client_dim.y * 0.7f);
        UI_Box *container = ui_make_box_from_key(UI_BoxFlag_DrawBackground, 0);

        UI_Parent(container)
        {
            ui_set_next_pref_width(ui_pct(1.f, 0.f));
            ui_set_next_pref_height(ui_px(default_fonts[FONT_DEFAULT]->glyph_height, 1.f));
            ui_set_next_child_layout_axis(Axis_X);
            UI_Box *fs_header = ui_make_box_from_key(UI_BoxFlag_Nil, 0);
            UI_Parent(fs_header)
            {
                ui_set_next_pref_width(ui_pct(1.f, 0.f));
                ui_set_next_pref_height(ui_text_dim(2.f, 1.f));
                u64 len = fs_state->path_len;
                UI_Signal prompt_sig = ui_line_edit(str8_lit("###prompt"), fs_state->path_buffer, ArrayCount(fs_state->path_buffer), &fs_state->path_pos, &fs_state->path_len);
                if (ui_pressed(prompt_sig)) {
                    if (prompt_sig.key == OS_KEY_SLASH || prompt_sig.key == OS_KEY_BACKSLASH) {
                        reload_files = true;
                    } else if (prompt_sig.key == OS_KEY_BACKSPACE && (fs_state->path_buffer[len-1] == '/' || fs_state->path_buffer[len-1] == '\\')) {
                        reload_files = true;
                    }
                }

                ui_set_next_pref_width(ui_text_dim(4.f, 1.f));
                ui_set_next_pref_height(ui_text_dim(2.f, 1.f));
                ui_button(str8_lit("Submit"));

                ui_set_next_pref_width(ui_text_dim(4.f, 1.f));
                ui_set_next_pref_height(ui_text_dim(2.f, 1.f));
                if (ui_clicked(ui_button(str8_lit("X")))) {
                    g_app_state->file_system_active = false;
                }
            }

            for (s32 file_idx = 0; file_idx < fs_state->cached_file_count; file_idx += 1) {
                OS_File file = fs_state->cached_files[file_idx];
                ui_set_next_pref_width(ui_pct(1.f, 1.f));
                ui_set_next_pref_height(ui_text_dim(2.f, 1.f));
                ui_set_next_child_layout_axis(Axis_X);
                UI_Signal file_sig = ui_buttonf("###%d_file", file_idx);
                UI_Parent(file_sig.box) {
                    ui_set_next_pref_width(ui_text_dim(4.f, 1.f));
                    ui_set_next_pref_height(ui_text_dim(4.f, 1.f));
                    ui_set_next_text_alignment(UI_TextAlign_Left);
                    ui_set_next_font(default_fonts[FONT_ICON]);
                    String8 file_icon_string = ui_string_from_icon_kind(file.flags & OS_FileFlag_Directory ? UI_IconKind_Folder : UI_IconKind_File, "###file_icon");
                    ui_label(file_icon_string);

                    ui_set_next_pref_width(ui_text_dim(4.f, 1.f));
                    ui_set_next_pref_height(ui_text_dim(4.f, 1.f));
                    ui_set_next_text_alignment(UI_TextAlign_Left);
                    ui_labelf("%s", file.file_name.data);
                }

                if (ui_clicked(file_sig)) {
                    String8 file_path = str8(fs_state->path_buffer, fs_state->path_len);
                    String8 new_path = path_join(scratch, file_path, file.file_name);
                    if (file.flags & OS_FileFlag_Directory) {
                        file_system_load_files(fs_state, new_path);
                    } else {
                        // Asset *asset = asset_load(new_path);
                        // set_current_asset(asset); 
                        // g_app_state->file_system_active = false;
                    }
                }
            }
        }
        arena_release(scratch);

        if (reload_files) {
            file_system_load_files(fs_state, str8(fs_state->path_buffer, fs_state->path_len));
        }
    }

    //@Note Update camera
    {
        Camera_2D prev_camera = g_app_state->camera;
        Camera_2D camera = prev_camera;

        f32 zoom_factor = 0.1f;
        s32 delta = g_input.scroll_delta.y;
        if (delta != 0) {
            camera.zoom = prev_camera.zoom * (1 + delta * zoom_factor);
        }
        camera.zoom = ClampBot(camera.zoom, 0);

        // v2 mouse = v2_subtract(input.mouse, window_center);
        // position = v2_subtract(position, mouse);
        // position = v2_mul_s(position, zoom / last_zoom);
        // position = v2_add(position, mouse);

        if (!ui_mouse_captured() && key_down(OS_KEY_LEFTMOUSE)) {
            v2 drag_delta = get_mouse_drag_delta();
            drag_delta.y *= -1.f;
            camera.position = prev_camera.position + drag_delta;
        }

        //@Note clamp position
        // v2_s32 asset_size = v2s32(1, 1);
        // if (g_app_state->current_asset) {
        //     asset_size = r_texture_size(g_app_state->current_asset->textures[0]);
        // }
        // camera.position.x = Clamp(camera.position.x, -asset_size.x/2.f, asset_size.x/2.f);
        // camera.position.y = Clamp(camera.position.y, -asset_size.y/2.f, asset_size.y/2.f);

        g_app_state->camera = camera;
    }
    
    //@Note Display image
    if (g_app_state->current_asset) {
        v3 camera_position = V3(g_app_state->camera.position.x + client_dim.x * 0.5f, g_app_state->camera.position.y + client_dim.y * 0.5f, 0.f);
        f32 zoom = g_app_state->camera.zoom;
        m4 camera_scale = scale_m4(V3(zoom, zoom, 1.f));
        m4 camera_trans = translate_m4(camera_position);
        m4 projection = ortho_rh_zo(0.f, client_dim.x, 0.f, client_dim.y, -1.f, 1.f);
        
        Asset *asset = g_app_state->current_asset;
        R_Handle tex = asset->textures[0];
        v2_s32 size = r_texture_size(tex);
        m4 scale = scale_m4(V3((f32)size.x, (f32)size.y, 1.f));

        m4 xform = identity_m4();
        xform = mul_m4(xform, scale);
        xform = mul_m4(xform, camera_scale);
        xform = mul_m4(xform, camera_trans);
        xform = mul_m4(xform, projection);
        draw_set_xform(xform);
        draw_set_sampler(g_app_state->point_sample ? R_SamplerKind_Point : R_SamplerKind_Linear);
    
        draw_quad_pro(tex, make_rect(0.f, 0.f, 1.f, 1.f), make_rect(-0.5f, -0.5f, 1.f, 1.f), V2(0.5f, 0.5f), g_app_state->rot, V4(1.f, 1.f, 1.f, 1.f));
    }

    ui_layout_apply(ui_root());

    u64 counter = get_wall_clock();

    draw_ui_layout(ui_root());

    // UI_Box_Node *node = nullptr;
    // node = push_array(ui_build_arena(), UI_Box_Node, 1);
    // node->box = ui_root();
    // while (node != nullptr) {
    //     UI_Box *box = node->box;
    //     draw_ui_box(box);
    //     node = node->next;
    // }

    f32 draw_ms = get_ms_elapsed(counter, get_wall_clock());

    d3d11_render(window_handle, draw_bucket);

    ui_end_build();

    draw_end();

    //@Note Input end
    g_input.scroll_delta = v2s32(0, 0);
    g_input.mouse_drag_start = g_input.mouse_position;
    for (int i = 0; i < ArrayCount(g_input.buttons); i++) {
        g_input.buttons[i] &= ~ButtonState_Released;
        g_input.buttons[i] &= ~ButtonState_Pressed;
    }
}
