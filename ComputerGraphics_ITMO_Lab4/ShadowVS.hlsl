struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
    float4 Color : COLOR;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

cbuffer cbObject : register(b0) {
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float4 gCameraPosition;
    float4 gTessellationParams;
    float4 gWaveParams;
};

cbuffer cbShadow : register(b1) {
    float4x4 gShadowViewProj;
};

float4 VS(VS_INPUT input) : SV_POSITION {
    float4 worldPos = mul(float4(input.Pos, 1.0f), gWorld);
    return mul(worldPos, gShadowViewProj);
}
