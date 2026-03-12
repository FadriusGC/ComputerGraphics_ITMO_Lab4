struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VS_OUTPUT VS(uint vertexID : SV_VertexID) {
    VS_OUTPUT output;

    float2 pos;
    pos.x = (vertexID == 2) ? 3.0f : -1.0f;
    pos.y = (vertexID == 1) ? 3.0f : -1.0f;

    output.Pos = float4(pos, 0.0f, 1.0f);
    output.TexC = float2((pos.x + 1.0f) * 0.5f, 1.0f - (pos.y + 1.0f) * 0.5f);

    return output;
}
