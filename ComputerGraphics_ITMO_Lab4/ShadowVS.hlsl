cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float4 gCameraPosition;
    float4 gTessellationParams;
    float4 gWaveParams;
};

cbuffer cbShadowPass : register(b1) {
    float4x4 gLightViewProj;
};

cbuffer cbMaterial : register(b2) {
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float gHasNormalMap;
    float gHasDisplacementMap;
    float gHasRoughnessMap;
    float gDisplacementScale;
    float gMetallic;
    float gHasMetallicMap;
    float2 gMatPadding;
    float4x4 gTexTransform;
};

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

struct VSInput {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
    float4 Color : COLOR;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct VSOutput {
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VSOutput VS(VSInput vin) {
    VSOutput vout;
    float4 posW = mul(float4(vin.Pos, 1.0f), gWorld);
    vout.PosH = mul(posW, gLightViewProj);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}

void PS(VSOutput pin) {
    float alpha = gDiffuseAlbedo.a * gDiffuseMap.Sample(gSampler, pin.TexC).a;
    clip(alpha - 0.1f);
}