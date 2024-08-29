cbuffer Constants : register(b0) {
    matrix xform;
};

struct Vertex_In {
    float2 dst    : POSITION;
    float2 src    : TEXCOORD;
    float4 color  : COLOR;
    float4 style_params : STY; // x = omit_texture,
};

struct Vertex_Out {
    float4 pos_h  : SV_POSITION;
    float2 tex    : TEXCOORD;
    float4 color  : COLOR;
    float omit_texture : OTX;
};

Texture2D tex : register(t0);
SamplerState tex_sampler : register(s0);

Vertex_Out vs_main(Vertex_In input) {
    Vertex_Out output;
    output.pos_h = mul(xform, float4(input.dst, 0.0, 1.0));
    output.tex = input.src;
    output.color = input.color;
    output.omit_texture = input.style_params.x;
    return output;
}

float4 ps_main(Vertex_Out input) : SV_TARGET {
    float4 tex_color = tex.Sample(tex_sampler, input.tex);
    float4 color;
    if (input.omit_texture) {
        color = input.color;
        color.a *= tex_color.a;
    } else {
        color = tex_color;
    }
    return color;
}
