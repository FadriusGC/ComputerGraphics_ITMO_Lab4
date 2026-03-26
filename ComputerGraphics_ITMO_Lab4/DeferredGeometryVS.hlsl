struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexC : TEXCOORD;
    float4 Color : COLOR;
};

struct VS_OUTPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexC : TEXCOORD;
};

VS_OUTPUT VS(VS_INPUT input) {
    VS_OUTPUT output;
    output.Pos = input.Pos;
    output.Normal = input.Normal;
    output.Tangent = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.TexC = input.TexC;
    return output;
}