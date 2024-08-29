#ifndef RENDER_CORE_H
#define RENDER_CORE_H

typedef u64 R_Handle;

enum R_Sampler_Kind {
    R_SamplerKind_Linear,
    R_SamplerKind_Point,
    R_SamplerKind_COUNT
};

enum R_Tex2D_Format {
    R_Tex2DFormat_R8,
    R_Tex2DFormat_R8G8B8A8,
};

struct R_2D_Vertex {
    v2 dst;
    v2 src;
    v4 color;
    f32 omit_tex;
    v3 padding_;
};

enum R_Params_Kind {
    R_ParamsKind_Nil,
    R_ParamsKind_UI,
    R_ParamsKind_Quad,
    R_ParamsKind_COUNT,
};

struct R_Params_UI {
    m4 xform;
    Rect clip;
    R_Handle tex;
};

struct R_Params_Quad {
    m4 xform;
    R_Handle tex;
    R_Sampler_Kind sampler;
};

struct R_Params {
    R_Params_Kind kind;
    union {
        R_Params_UI *params_ui;
        R_Params_Quad *params_quad;
    };
};

struct R_Batch {
    R_Params params;
    u8 *v;
    int bytes;
};

struct R_Batch_Node {
    R_Batch_Node *next;
    R_Batch batch;
};

struct R_Batch_List {
    R_Batch_Node *first;
    R_Batch_Node *last;
    int count;
};

#endif // RENDER_CORE_H
