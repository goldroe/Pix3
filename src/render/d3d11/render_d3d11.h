#ifndef RENDER_D3D11_H
#define RENDER_D3D11_H

#include <d3d11.h>
#include <d3dcompiler.h>

struct R_D3D11_Tex2D {
    R_D3D11_Tex2D *next;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *view;
    R_Tex2D_Format format;
    v2_s32 size;
};

enum D3D11_Shader_Kind {
    D3D11_ShaderKind_UI,
    D3D11_ShaderKind_Quad,
    D3D11_ShaderKind_COUNT
};

enum D3D11_Sampler_Kind {
    D3D11_SamplerKind_Linear,
    D3D11_SamplerKind_Point,
    D3D11_SamplerKind_COUNT
};

struct D3D11_Uniform_UI {
    m4 xform;
};

struct D3D11_Uniform_Quad {
    m4 xform;
};

struct R_D3D11_State {
    Arena *arena;
    IDXGISwapChain *swap_chain;
    ID3D11Device *device;
    ID3D11DeviceContext *device_context;

    //@Note Resources
    ID3D11RenderTargetView *render_target_view;
    ID3D11DepthStencilState *depth_stencil_state;
    ID3D11DepthStencilView *depth_stencil_view;
    ID3D11RasterizerState *rasterizer_state;
    ID3D11BlendState *blend_state;
    ID3D11SamplerState *samplers[D3D11_SamplerKind_COUNT];
    ID3D11VertexShader *vertex_shaders[D3D11_ShaderKind_COUNT];
    ID3D11PixelShader *pixel_shaders[D3D11_ShaderKind_COUNT];
    ID3D11InputLayout *input_layouts[D3D11_ShaderKind_COUNT];
    ID3D11Buffer *uniform_buffers[D3D11_ShaderKind_COUNT];

    R_D3D11_Tex2D *first_free_tex2d;

    //@Note Immediate context
    Rect draw_region;
    R_Handle fallback_tex;
};

#endif //RENDER_D3D11_H
