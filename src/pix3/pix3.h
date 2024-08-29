#ifndef PIX3_H
#define PIX3_H

enum Asset_Kind {
    AssetKind_Nil,
    AssetKind_Image,
    AssetKind_Animation,
};

struct Asset {
    Asset *prev;
    Asset *next;
    Asset_Kind kind;
    
    OS_File *file;
    f32 frame_t;
    f32 duration;
    R_Handle *textures;
    u8 *bitmap;
    s64 texture_count;
};

struct Asset_List {
    Asset *first;
    Asset *last;
    s32 count;
};

struct File_System_State {
    u8  path_buffer[1024];
    u64 path_len;
    u64 path_pos;
    
    // String8 normalized_path;
    Arena *cached_files_arena;
    OS_File *cached_files;
    s32 cached_file_count;
};

struct Camera_2D {
    v2 position;
    f32 rotation;
    f32 zoom;
};

struct App_State {
    Arena *arena;
    String8 current_directory;

    File_System_State *fs_state;
    b32 file_system_active;

    UI_Scroll_Pt film_strip_scroll_pt;
    b32 film_strip_active;

    //@Note Asset files for current directory
    // Arena *cached_files_arena;
    // s32 cached_file_count;
    // OS_File *cached_files;
    // s32 current_file_index;

    Arena *asset_arena;
    Asset_List assets;
    Asset *current_asset;

    Camera_2D camera;
    b32 point_sample;
};

#endif // PIX3_H
