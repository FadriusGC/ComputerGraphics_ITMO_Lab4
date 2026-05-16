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
};

VSOutput VS(VSInput vin) {
    VSOutput vout;
    float4 posW = mul(float4(vin.Pos, 1.0f), gWorld);
    vout.PosH = mul(posW, gLightViewProj);
    return vout;
}