struct Particle
{
    float3 Position;
    float Age;
    float3 Velocity;
    float Lifetime;
    float4 Color;
    float Size;
    float3 Padding;
};

StructuredBuffer<Particle> gParticlePool : register(t0);

cbuffer ParticleRenderCB : register(b0)
{
    float4x4 gViewProj;
    float3 gCameraPosition;
    float gBillboardSize;
    uint gMaxParticles;
    float3 gPad;
};

struct VSOut
{
    uint ParticleIndex : PARTICLEINDEX;
};

struct GSOut
{
    float4 PositionH : SV_POSITION;
    float2 Uv : TEXCOORD;
    float4 Color : COLOR0;
};

[maxvertexcount(4)]
void GS(point VSOut input[1], inout TriangleStream<GSOut> triStream)
{
    uint idx = input[0].ParticleIndex;
    if (idx >= gMaxParticles) return;

    Particle p = gParticlePool[idx];
    if (p.Age < 0.0 || p.Age >= p.Lifetime) return;

    float3 toCam = normalize(gCameraPosition - p.Position);
    float3 upAxis = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(upAxis, toCam));
    float3 up = normalize(cross(toCam, right));

    float halfSize = p.Size * gBillboardSize;

    float3 corners[4];
    corners[0] = p.Position + (-right + up) * halfSize;
    corners[1] = p.Position + ( right + up) * halfSize;
    corners[2] = p.Position + (-right - up) * halfSize;
    corners[3] = p.Position + ( right - up) * halfSize;

    float2 uvs[4] = {
        float2(0.0, 0.0), float2(1.0, 0.0),
        float2(0.0, 1.0), float2(1.0, 1.0)
    };

    GSOut o;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        o.PositionH = mul(float4(corners[i], 1.0), gViewProj);
        o.Uv = uvs[i];
        o.Color = p.Color;
        triStream.Append(o);
    }
}
