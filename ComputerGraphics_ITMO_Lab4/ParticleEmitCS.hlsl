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

ConsumeStructuredBuffer<uint> gDeadListConsume : register(u2);
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

[numthreads(64,1,1)]
void CS(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= gSpawnCount) return;

    uint particleIndex = gDeadListConsume.Consume();

    float seed = frac(sin((dtid.x + 1) * 12.9898 + gTotalTime * 78.233) * 43758.5453);
    float seed2 = frac(sin((dtid.x + 1) * 63.7264 + gTotalTime * 17.371) * 12345.6789);

    Particle p;
    p.Position = gEmitterPosition + float3((seed - 0.5) * gEmitterSpread, 0.2, (seed2 - 0.5) * gEmitterSpread);
    p.Velocity = float3((seed - 0.5) * 1.5, 5.5 + seed2 * 3.5, (seed2 - 0.5) * 1.5);
    p.Age = 0.0;
    p.Lifetime = 2.0 + seed * 1.4;
    p.Color = float4(1.0, 0.07, 0.07, 1.0);
    p.Size = 0.35 + seed2 * 0.2;
    p.Padding = 0.0.xxx;

    gParticlePool[particleIndex] = p;
}
