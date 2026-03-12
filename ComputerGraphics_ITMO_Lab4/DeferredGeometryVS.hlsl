struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
    float4 Color : COLOR;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
};

cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
    float4x4 gWorldViewProj;
};

VS_OUTPUT VS(VS_INPUT input) {
    VS_OUTPUT output;

    float4 worldPos = mul(float4(input.Pos, 1.0f), gWorld);
    output.Pos = mul(worldPos, gWorldViewProj);
    output.WorldPos = worldPos.xyz;
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), gWorld).xyz);
    output.TexC = input.TexC;

    return output;
}