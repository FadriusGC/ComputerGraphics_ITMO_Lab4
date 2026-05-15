cbuffer ObjectCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float4 gCameraPosition;
    float4 gTessellationParams;
    float4 gWaveParams;
};

cbuffer ShadowFrameCB : register(b1)
{
    float4x4 gShadowViewProj;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

struct VSOut { float4 pos : SV_POSITION; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 worldPos = mul(float4(i.pos, 1.0f), gWorld);
    o.pos = mul(worldPos, gShadowViewProj);
    return o;
}

void PSMain() {}
