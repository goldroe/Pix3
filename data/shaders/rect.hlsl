cbuffer Constants : register(b0) {
    matrix xform;
};

struct Vertex_In {
    float4 dst_rect    : POS;
    float4 src_rect    : TEX;
    float4 color       : COL;
    nointerpolation float4 style : STY;  //x=omit_tex
    uint vertex_id     : SV_VertexID;
};

struct Vertex_Out {
    float4 pos            : SV_POSITION;
    nointerpolation float2 rect_half_size_px : PSIZE;
    float2 sdf_sample_pos : SDF;
    float2 src            : TEX;
    float4 color          : COL;
    nointerpolation float border_thickness_px  : BTP;
    nointerpolation float omit_tex             : OTX;
};

Texture2D main_tex : register(t0);
SamplerState tex_sampler : register(s0);

float rect_sdf(float2 sample_pos, float2 size, float radius) {
    return length(max(abs(sample_pos) - size + radius, 0.0)) - radius;
}

Vertex_Out vs_main(Vertex_In vertex) {
    float2 dst_p0 = vertex.dst_rect.xy;
    float2 dst_p1 = vertex.dst_rect.zw;
    float2 src_p0 = vertex.src_rect.xy;
    float2 src_p1 = vertex.src_rect.zw;

    float2 dst_verts_px[] = {
        float2(dst_p0.x, dst_p1.y),
        float2(dst_p0.x, dst_p0.y),
        float2(dst_p1.x, dst_p1.y),
        float2(dst_p1.x, dst_p0.y)
    };
    float2 src_verts_px[] = {
        float2(src_p0.x, src_p1.y),
        float2(src_p0.x, src_p0.y),
        float2(src_p1.x, src_p1.y),
        float2(src_p1.x, src_p0.y)
    };

    float2 dst_size_px = abs(dst_p1 - dst_p0);
    float2 dst_verts_pct = float2((vertex.vertex_id >> 1) ? 1.f : 0.f,
                                 (vertex.vertex_id & 1)  ? 0.f : 1.f);

    float border_thickness_px = vertex.style.x;
    float omit_tex = vertex.style.y;

    Vertex_Out vertex_out;
    vertex_out.pos = mul(xform, float4(dst_verts_px[vertex.vertex_id], 0, 1));
    vertex_out.src = src_verts_px[vertex.vertex_id];
    vertex_out.color = vertex.color;
    vertex_out.rect_half_size_px = dst_size_px / 2.f;
    vertex_out.sdf_sample_pos = (2.f * dst_verts_pct - 1.f) * vertex_out.rect_half_size_px;
    vertex_out.omit_tex = omit_tex;
    vertex_out.border_thickness_px = border_thickness_px;
    return vertex_out;
}

float4 ps_main(Vertex_Out vertex) : SV_TARGET {
    float4 tint = vertex.color;
    float4 albedo_sample = float4(1, 1, 1, 1);
    if (vertex.omit_tex < 1.f) {
        albedo_sample = main_tex.Sample(tex_sampler, vertex.src);
    }

    //@Note SDF border
    float softness = 1.f;
    float2 sdf_sample_pos = vertex.sdf_sample_pos;
    float border_sdf_t = 1;
    if (vertex.border_thickness_px > 0) {
        float border_sdf_s = rect_sdf(sdf_sample_pos, vertex.rect_half_size_px, vertex.border_thickness_px);
        border_sdf_t = 1.f - smoothstep(0, softness * 2.f, border_sdf_s);
    }

    if (border_sdf_t < 0.001f) {
        discard;
    }

    float4 final_color;
    final_color = albedo_sample;
    final_color *= tint;
    final_color.a *= border_sdf_t;
    return final_color;
}
