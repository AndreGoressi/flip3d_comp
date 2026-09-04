#pragma once

// ============================================================================
// HLSL shader source — card quad only (Stage 2 test rig).
// Ported from ALTaleX531/flip3d's Shaders.h.
// ============================================================================

inline constexpr const char* kCardVertexShader = R"(
cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 world;
};

struct VSIn
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut main(VSIn input)
{
    VSOut output;
    float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, viewProj);
    output.uv = input.uv;
    return output;
}
)";

inline constexpr const char* kCardPixelShader = R"(
Texture2D<float4> cardTexture : register(t0);
SamplerState cardSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    // Captured window content is already premultiplied-alpha friendly (opaque
    // window content -> alpha 1); premultiply anyway for DXGI_ALPHA_MODE_PREMULTIPLIED.
    float4 windowColor = cardTexture.Sample(cardSampler, uv);
    float alpha = windowColor.a;
    return float4(windowColor.rgb * alpha, alpha);
}
)";
