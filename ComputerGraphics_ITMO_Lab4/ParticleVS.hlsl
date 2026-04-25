struct VSOut
{
    uint ParticleIndex : PARTICLEINDEX;
};

VSOut VS(uint vertexId : SV_VertexID)
{
    VSOut o;
    o.ParticleIndex = vertexId;
    return o;
}
