#ifndef DRAW_H
#define DRAW_H

struct Draw_Bucket {
    m4 xform;
    R_Handle tex;
    Rect clip;
    R_Params_Kind params_kind;
    R_Batch_List batches;
    R_Sampler_Kind sampler;
};

internal void draw_begin(OS_Handle window_handle);
internal void draw_end();

internal void draw_quad(R_Handle img, Rect dst, Rect src);
internal void draw_rect(Rect dst, v4 color);
internal void draw_string(String8 string, Font *font, v4 color, v2 offset);

#endif // DRAW_H
