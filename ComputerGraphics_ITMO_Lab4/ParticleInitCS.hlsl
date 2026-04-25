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

RWStructuredBuffer<Particle> gParticlePool : register(u0);
AppendStructuredBuffer<uint> gDeadListAppend : register(u1);

cbuffer SimCB : register(b0)
{
    float gDeltaTime;
    float gTotalTime;
    uint gSpawnCount;
    uint gMaxParticles;
    float3 gEmitterPosition;
    float gEmitterSpread;
};

[numthreads(128,1,1)]
void CS(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= gMaxParticles) return;

    Particle p;
    p.Position = 0.0.xxx;
    p.Age = -1.0;
    p.Velocity = 0.0.xxx;
    p.Lifetime = 0.0;
    p.Color = float4(1.0, 0.0, 0.0, 1.0);
    p.Size = 0.4;
    p.Padding = 0.0.xxx;
    gParticlePool[idx] = p;
    gDeadListAppend.Append(idx);
}
