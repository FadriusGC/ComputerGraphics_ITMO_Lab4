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

StructuredBuffer<uint> gDeadListConsume : register(t0);
StructuredBuffer<Particle> gParticlePoolSRV : register(t1);
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

    Particle p = gParticlePool[idx];
    if (p.Age < 0.0)
    {
        return;
    }

    p.Age += gDeltaTime;
    p.Velocity += float3(0.0, -9.8, 0.0) * gDeltaTime;
    p.Position += p.Velocity * gDeltaTime;

    if (p.Age >= p.Lifetime || p.Position.y < -5.0)
    {
        p.Age = -1.0;
        gDeadListAppend.Append(idx);
    }

    gParticlePool[idx] = p;
}
