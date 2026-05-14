struct VSIn {
    float3 Pos : POSITION;
};

cbuffer cbObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float4 gCameraPosition;
    float4 gTessParams;
    float4 gWaveParams;
};

cbuffer cbShadow : register(b1)
{
    float4x4 gLightViewProj;
};

float4 VS(VSIn vin) : SV_POSITION
{
    float4 worldPos = mul(float4(vin.Pos, 1.0f), gWorld);
    return mul(worldPos, gLightViewProj);
}
