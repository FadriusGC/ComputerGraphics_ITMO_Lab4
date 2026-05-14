struct PSIn
{
    float4 PositionH : SV_POSITION;
    float2 Uv : TEXCOORD;
    float4 Color : COLOR0;
};

cbuffer ParticleRenderCB : register(b0)
{
    float4x4 gViewProj;
    float3 gCameraPosition;
    float gBillboardSize;
    uint gMaxParticles;
    float gTotalTime;
    float2 gAtlasGrid;
    float gAtlasFps;
    float gPad;
};

Texture2D gSmokeAtlas : register(t1);


float4 PS(PSIn input) : SV_TARGET
{
    
    uint columns = max((uint)gAtlasGrid.x, 1u);
    uint rows = max((uint)gAtlasGrid.y, 1u);
    uint frameCount = max(columns * rows, 1u);
    uint frame = (uint)(gTotalTime * gAtlasFps) % frameCount;
    uint2 tile = uint2(frame % columns, frame / columns);

    float2 atlasUv = (input.Uv + float2(tile)) / float2(columns, rows);
    atlasUv = saturate(atlasUv);

    uint width, height;
    gSmokeAtlas.GetDimensions(width, height);
    int2 texel = int2(atlasUv * float2(max((int)width - 1, 1), max((int)height - 1, 1)));
    float4 smoke = gSmokeAtlas.Load(int3(texel, 0));

    float alpha = smoke.a * input.Color.a;
    clip(alpha - 0.02f);
    return float4(smoke.rgb * input.Color.rgb, alpha);
}
